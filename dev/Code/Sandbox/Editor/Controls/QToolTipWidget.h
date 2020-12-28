/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#ifndef QToolTipWidget_h__
#define QToolTipWidget_h__

#include "EditorCoreAPI.h"

#include <QWidget>
#include <QLabel>
#include <QString>
#include <QMap>
#include <QMapIterator>
#include <QVector>
#include <QVBoxLayout>

#include <memory>

class IQToolTip
{
public:
    virtual void SetTitle(QString title) = 0;
    virtual void SetContent(QString content) = 0;
    virtual void AppendContent(QString content) = 0;
    virtual void AddSpecialContent(QString type, QString dataStream) = 0;
    virtual void UpdateOptionalData(QString optionalData) = 0;
};

class EDITOR_CORE_API QToolTipWidget
    : public QWidget
    , public IQToolTip
{
public:
    enum class ArrowDirection
    {
        ARROW_UP,
        ARROW_LEFT,
        ARROW_RIGHT,
        ARROW_DOWN
    };
    class QArrow
        : public QWidget
    {
    public:
        ArrowDirection m_direction;
        QPoint m_pos;
        QArrow(QWidget* parent)
            : QWidget(parent){ setWindowFlags(Qt::ToolTip); }
        virtual ~QArrow(){}

        QPolygonF CreateArrow();
        virtual void paintEvent(QPaintEvent*) override;
    };
    QToolTipWidget(QWidget* parent);
    ~QToolTipWidget();
    void SetTitle(QString title) override;
    void SetContent(QString content) override;
    void AppendContent(QString content) override;
    void AddSpecialContent(QString type, QString dataStream) override;
    void UpdateOptionalData(QString optionalData) override;
    void Display(QRect targetRect, ArrowDirection preferredArrowDir);

    //! Displays the tooltip on the given widget, only if the mouse is over it.
    void TryDisplay(QPoint mousePos, const QWidget* widget, ArrowDirection preferredArrowDir);

    //! Displays the tooltip on the given rect, only if the mouse is over it.
    void TryDisplay(QPoint mousePos, const QRect& widget, ArrowDirection preferredArrowDir);

    void Hide();
    
protected:
    void Show(QPoint pos, ArrowDirection dir);
    bool IsValid();
    void KeepTipOnScreen(QRect targetRect, ArrowDirection preferredArrowDir);
    QPoint AdjustTipPosByArrowSize(QPoint pos, ArrowDirection dir);
    virtual bool eventFilter(QObject* obj, QEvent* event) override;
    void RebuildLayout();
    virtual void hideEvent(QHideEvent*) override;

    QLabel* m_title;
    AZ_PUSH_DISABLE_DLL_EXPORT_MEMBER_WARNING
    QVector<QLabel*> m_currentShortcuts;
    AZ_POP_DISABLE_DLL_EXPORT_MEMBER_WARNING
    //can be anything from QLabel to QBitMapPreviewDialog
    //must allow movement, and show/hide calls
    QLabel* m_content;
    QWidget* m_specialContent;
    QWidget* m_background;
    QVBoxLayout* m_layout;
    QString m_special;
    QPoint m_normalPos;
    QArrow* m_arrow;
    const int m_shadowRadius = 5;
    bool m_includeTextureShortcuts; //added since Qt does not support modifier only shortcuts
};

// HACK: The EditorUI_QT classes all were keeping persistent references to QToolTipWidgets around
// This led to many, many top-level widget creations, which led to many platform-side window allocations
// which led to crashes in Qt5.15. As this is legacy code, this is a drop-in replacement that only
// allocates the actual QToolTipWidget (and thus platform window) while the tooltip is visible
class EDITOR_CORE_API QToolTipWrapper
    : public QObject
    , public IQToolTip
{
public:
    QToolTipWrapper(QWidget* parent);

    void SetTitle(QString title) override;
    void SetContent(QString content) override;
    void AppendContent(QString content) override;
    void AddSpecialContent(QString type, QString dataStream) override;
    void UpdateOptionalData(QString optionalData) override;

    void Display(QRect targetRect, QToolTipWidget::ArrowDirection preferredArrowDir);
    void TryDisplay(QPoint mousePos, const QWidget* widget, QToolTipWidget::ArrowDirection preferredArrowDir);
    void TryDisplay(QPoint mousePos, const QRect& widget, QToolTipWidget::ArrowDirection preferredArrowDir);
    void hide();
    void show();
    bool isVisible() const;
    void update();
    void repaint(){update();} //Things really shouldn't be calling repaint on these...

    void Hide(){hide();}
    void close(){hide();}

private:
    void ReplayContentOperations(QToolTipWidget* tooltipWidget);

    QToolTipWidget* GetOrCreateToolTip();
    void DestroyToolTip();

    QPointer<QToolTipWidget> m_actualTooltip;

    QString m_title;
    QVector<QPair<QString, QString>> m_contentOperations;
};


#endif // QToolTipWidget_h__
