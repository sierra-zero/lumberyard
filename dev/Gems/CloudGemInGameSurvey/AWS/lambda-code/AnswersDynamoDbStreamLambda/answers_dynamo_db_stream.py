#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
from __future__ import print_function

from six import iteritems  # Python 2.7/3.7 Compatibility
import survey_utils


def handle(event, context):
    answer_aggregation_table = survey_utils.get_answer_aggregation_table()
    question_map = {}
    for record in event['Records']:
        # DynamoDB event type (INSERT | MODIFY | REMOVE)
        eventName = record['eventName']
        dynamoRecord = record['dynamodb']
        survey_id = list(dynamoRecord['Keys']['survey_id'].values())[0]

        print(dynamoRecord)

        if eventName == 'INSERT':
            new_image = dynamoRecord['NewImage']
            for key, val in iteritems(list(new_image['answers'].values())[0]):
                question_id = key
                answers = [list(x.values())[0] for x in list(val.values())[0]]

                question = get_question(survey_id, question_id, question_map)
                if question is None or question.get('type') == 'text':
                    continue

                # create the answer aggregation map if not exists
                create_aggregated_map(answer_aggregation_table, survey_id, question_id)
                for answer in answers:
                    # +1 answer count
                    increase_answer_count(answer_aggregation_table, survey_id, question_id, answer)
        elif eventName == 'MODIFY':
            new_image = dynamoRecord['NewImage']
            old_image = dynamoRecord['OldImage']
            for key, val in iteritems(list(new_image['answers'].values())[0]):
                question_id = key
                answers = map(lambda x: list(x.values())[0], list(val.values())[0])

                question = get_question(survey_id, question_id, question_map)
                if question is None or question.get('type') == 'text':
                    continue

                # create the answer aggregation map if not exists
                create_aggregated_map(answer_aggregation_table, survey_id, question_id)

                old_answers_map = list(old_image['answers'].values())[0]
                old_answers = map(lambda x: list(x.values())[0], list(old_answers_map[question_id].values())[0]) if question_id in old_answers_map else None

                if old_answers is not None:
                    new_answers_set = set(answers)
                    for old_answer in old_answers:
                        if old_answer not in new_answers_set:
                            # -1 old answer count
                            decrease_answer_count(answer_aggregation_table, survey_id, question_id, old_answer)

                    old_answers_set = set(old_answers)
                    for answer in answers:
                        if answer not in old_answers_set:
                            # +1 answer count
                            increase_answer_count(answer_aggregation_table, survey_id, question_id, answer)
                else:
                    for answer in answers:
                        # +1 answer count
                        increase_answer_count(answer_aggregation_table, survey_id, question_id, answer)


def get_question(survey_id, question_id, question_map):
    question = question_map.get(question_id)
    if question is None:
        question = survey_utils.get_question_by_id(survey_id, question_id, ['type'])
        if question is not None:
            question_map[question_id] = question
    return question


def create_aggregated_map(answer_aggregation_table, survey_id, question_id):
    answer_aggregation_table.update_item(
        Key={'survey_id':survey_id},
        UpdateExpression='SET #question_id = if_not_exists(#question_id, :empty_map)',
        ExpressionAttributeValues={':empty_map': {}},
        ExpressionAttributeNames={'#question_id': question_id}
    )


def increase_answer_count(answer_aggregation_table, survey_id, question_id, answer):
    answer_aggregation_table.update_item(
        Key={'survey_id': survey_id},
        UpdateExpression='ADD #question_id.#answer :one',
        ExpressionAttributeValues={':one': 1},
        ExpressionAttributeNames={'#question_id': question_id, '#answer': answer}
    )


def decrease_answer_count(answer_aggregation_table, survey_id, question_id, answer):
    answer_aggregation_table.update_item(
        Key={'survey_id': survey_id},
        UpdateExpression='ADD #question_id.#answer :minus_one',
        ExpressionAttributeValues={':minus_one': -1},
        ExpressionAttributeNames={'#question_id': question_id, '#answer': answer}
    )
