﻿const gulp = require('gulp');
const typescript = require('gulp-typescript');
const Builder = require('systemjs-builder');
const inlineNg2Template = require('gulp-inline-ng2-template');
const fs = require('fs');
const path = require('path');
const del = require('del');
const tsProject = typescript.createProject('tsconfig.json');
const rename = require('gulp-rename');
const sass = require('gulp-sass');
const sourcemaps = require('gulp-sourcemaps');
const prefix = require('gulp-autoprefixer');
const chmod = require('gulp-chmod');
const browserSync = require('browser-sync').create();
const nodemon = require('gulp-nodemon');
const log = require('fancy-log');
const colors = require('ansi-colors')
const uglify = require('gulp-uglify');
const protractor = require("gulp-protractor").protractor;
const webdriver_standalone = require("gulp-protractor").webdriver_standalone;
var webdriver_update = require('gulp-protractor').webdriver_update_specific;
const find = require('find-process');
const exec = require('gulp-exec');
const exec_process = require('child_process').exec;
const argv = require('yargs').argv;
const through2 = require('through2');
const open = require('gulp-open');
const PluginError = require('plugin-error');
const mkdirp = require('mkdirp');
const getDirName = require('path').dirname;

dist_path = "../../AWS/www/"
cgp_resource_code_folder = "cgp-resource-code"
cgp_resource_code_src_folder = "src"
cgp_package_name = "cloud-gem-portal"
package_scope_name = "@cloud-gems"
aws_folder = "AWS"
systemjs_config = "config-dist.js"
systemjs_config_dev = "config.js"
bootstrap_pattern = /{(?=.*"identityPoolId":\s"(\S*)")(?=.*"projectConfigBucketId":\s"(\S*)")(?=.*"userPoolId":\s"(\S*)")(?=.*"region":\s"(\S*)")(?=.*"clientId":\s"(\S*)").*}/g
port_pattern = /var port(\s)*=(\s)*':3000'/
domain_pattern = /var domain(\s)*=(\s)*'localhost'/
schema_pattern = /var schema(\s)*=(\s)*'http:\/\/'/
index_dist = "index-dist.html"
index = "index.html"
app_bundle = "bundles/app.bundle.js"
app_bundle_dependencies = "bundles/dependencies.bundle.js"
app_min_bundle = "bundles/app.bundle.min.js"
app_min_bundle_dependencies = "bundles/dependencies.bundle.min.js"
environment_file = "./app/shared/class/environment.class.ts"
localhost_port = 3000

const test_constants = {
    // The default admin user name if none is provided
    default_administrator_user: 'administrator'
}

const paths = {
    styles: {
        files: ['./app/**/*.scss', './styles/**/*.scss', './node_modules/@cloud-gems/**/*.scss'],
        dest: '.'
    },
    gems: {
        src: './node_modules/@cloud-gems',
        files: './node_modules/@cloud-gems/**/*.ts',
        relativeToGemsFolder: path.join(path.join(path.join("..", ".."), ".."), "..")
    }
}

//gulp-exec options
var gulp_exec = {
    options: {
        continueOnError: false, // default = false, true means don't emit error event
        pipeStdout: true // default = false, true means stdout is written to file.contents
    },
    reportOptions: {
        err: true, // default = true, false means don't write err
        stderr: true, // default = true, false means don't write stderr
        stdout: true // default = true, false means don't write stdout
    }
};

// A display error function, to format and make custom errors more uniform
// Could be combined with gulp-util or npm colors for nicer output
var displayError = function (error) {
    // Initial building up of the error
    var errorString = '[' + error.plugin + ']';
    errorString += ' ' + error.message.replace("\n", ''); // Removes new line at the end
    // If the error contains the filename or line number add it to the string
    if (error.fileName)
        errorString += ' in ' + error.fileName;
    if (error.lineNumber)
        errorString += ' on line ' + error.lineNumber;
    // This will output an error like the following:
    // [gulp-sass] error message in file_name on line 1

    log.error(new Error(errorString));
}

// Clean the contents of the distribution directory including 3rd party dependencies
gulp.task('clean:bundles', function () {
    return del('bundles/**/*', {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
});

// Clean the contents of the gems distribution directories
gulp.task('clean:dist', function () {
    return del('dist/**/*', {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
});

// Clean up all gems compiled code
gulp.task('clean:gems', function () {
    return del(paths.gems.src + '/**/*.js', {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
})

// Clean up system js files
gulp.task('clean:assets', function () {
    return del(systemjs_config, {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
})

// Clean up any files in the AWS www framework folder
gulp.task('clean:www', function (cb) {
    del(dist_path + "**/*.js", {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
    del(dist_path + "**/*.css", {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
    del(dist_path + "**/*.html", {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
    del(dist_path + "**/*.json", {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
    del(dist_path + "**/*.map", {force: true}).then(function (paths) {
        console.log('Deleted files and folders:\n', paths.join('\n'));
    });
    cb()
});

// Run all the clean tasks in series
exports.clean = gulp.series(
    'clean:assets',
    'clean:bundles',
    'clean:dist',
    'clean:gems',
    'clean:www'
);

// Run Sass build step to generate CSS files
gulp.task('sass', function () {
    // Taking the path from the above object
    return gulp.src(paths.styles.files, {base: "./"})
        // Sass options - make the output compressed and add the source map
        // Also pull the include path from the paths object
        .pipe(sass({outputStyle: 'compressed'}))
        // If there is an error, don't stop compiling but use the custom displayError function
        .on('error', function (err) {
            displayError(err);
        })
        // Pass the compiled sass through the prefixer with defined
        .pipe(prefix(
            'last 2 version', 'safari 5', 'ie 8', 'ie 9', 'opera 12.1', 'ios 6', 'android 4'
        ))
        .pipe(chmod(666))
        .pipe(gulp.dest("./", {overwrite: true, force: true}));
});

// Link the CloudGemPortal cloud gem packages as node modules
gulp.task('npm:link', function () {
    let cwd = process.cwd();
    return cloudGems()
        .pipe(through2.obj(function (file, enc, callback) {
            var gem_path = file.path.substring(0, file.path.lastIndexOf(path.sep) + 1)
            var names = packageName(file.path);
            var package_name = names[0]
            var gem_name = names[1]
            if (gem_name === "cloudgemportal") {
                callback(null, file)
                return;
            }

            log("")
            log(colors.blue("GLOBAL LINKING"));
            log("Package Name:\t\t" + package_name);
            log("Gem Name:\t\t\t" + gem_name);
            log("Path:\t\t\t" + gem_path);
            log(gem_path)
            process.chdir(gem_path)
            var stream = this.push
            exec_process('npm link')
                .on("close", function (code, cb) {
                    log("LINKING global package complete for '" + package_name + "'")
                    log("")
                    process.chdir(cwd)
                    if (fs.existsSync("node_modules/@cloud-gems/" + gem_name)) {
                        callback(null, file)
                        return;
                    }
                    log(colors.yellow("LOCAL LINKING"))
                    log("LINKING package " + cgp_package_name + " to package " + package_name + ".");
                    exec_process('npm link @cloud-gems/' + gem_name)
                        .on("close", function (code) {
                            log("LINKING CGP Cloud gem package to '" + package_name + "' is complete")
                            log("")
                            callback(null, file)
                        })
                });
        }))
})

// Ensure an environment file exists for the current process
var verifyEnvironmentFile = function () {
    var exists = fs.existsSync(environment_file)
    if (!exists) {
        writeFile(environment_file, "")
    }
}

// Set the prod flag to true in the environment file
gulp.task('set:prod-define', function (cb) {
    verifyEnvironmentFile()
    globalDefines()
    replaceOrAppend(environment_file, /export const isProd: boolean = (true|false)/g, "export const isProd: boolean = true")
    cb()
});

// Set the test flag to true in the environment file
gulp.task('set:test-define-on', function (cb) {
    verifyEnvironmentFile()
    globalDefines()
    replaceOrAppend(environment_file, /export const isTest: boolean = (true|false)/g, "export const isTest: boolean = true")
    cb()
});

// Set the test flag to false in the environment file
gulp.task('set:test-define-off', function (cb) {
    verifyEnvironmentFile()
    globalDefines()
    replaceOrAppend(environment_file, /export const isTest: boolean = (true|false)/g, "export const isTest: boolean = false")
    cb()
});

// Set the development flag to false in the environment file
gulp.task('set:dev-define', function (cb) {
    verifyEnvironmentFile()
    globalDefines()
    replaceOrAppend(environment_file, /export const isProd: boolean = (true|false)/g, "export const isProd: boolean = false")
    cb()
});

// Typescript compilation for gems in development
gulp.task('gem-ts', function () {
    log("COMPILING files in directory '" + process.cwd() + path.sep + paths.gems.files + "'");
    return gulp.src(paths.gems.files, {base: "./"})
        .pipe(sourcemaps.init())
        .pipe(tsProject())
        .pipe(gulp.dest("./"));
});

// Watch gem folders for changes to source files
gulp.task('gem-watch', function () {
    return cloudGems()
        .pipe(through2.obj(function (file, encoding, done) {
            const gem_path = path.dirname(file.path)
            log("WATCHING [*.ts, *.scss] files in directory '" + gem_path + "'");
            gulp.watch(gem_path + "/**/*.ts", {
                depth: 10,
                delay: 100,
                followSymlinks: true,
                ignoreInitial: true
            }, gulp.series('gem-ts'))
                .on('change', function (path, evt) {
                    log(
                        '[watcher] File ' + path.replace(/.*(?=ts)/, '') + ' was changed, compiling...'
                    )
                })
            gulp.watch(gem_path + "/**/*.scss", {
                depth: 10,
                delay: 100,
                followSymlinks: true,
                ignoreInitial: true
            }, gulp.series('sass'))
                .on('change', function (path, evt) {
                    log(
                        '[watcher] File ' + path.replace(/.*(?=scss)/, '') + ' was changed, compiling...'
                    )
                })
            done()
        }))

});

var initializeBootstrap = function (cb) {
    //is the bootstrap set
    var cwd = process.cwd();
    var bootstrap = fs.readFileSync(index, 'utf-8').toString();
    log(colors.yellow(`Looking for a local Cloud Gem Portal bootstrap variable in location ${cwd + path.sep + index}.`))
    var match = bootstrap.match(bootstrap_pattern)
    if (match) {
        //a local bootstrap has been defined
        log(colors.yellow(`Bootstrap found at path ${cwd + index}.`))
        startServer()
        cb()
    } else {
        //no bootstrap has been set.   Attempting to stand up a project stack
        process.chdir(enginePath())
        exec_process('lmbr_aws project list-resources', function (err, stdout, stderr) {
            log(stdout);
            process.chdir(cwd)
            let match = stdout.match(/CloudGemPortal\s*AWS::S3::Bucket\s*[CREATE_COMPLETE|UPDATE_COMPLETE]/i)
            // is there a project stack created?
            if (match) {
                // yes a project stack exists, create the local bootstrap
                writeBootstrapAndStart(cb)
            } else {
                // no
                log(colors.yellow("No project stack or bootstrap is defined."))
                createProjectStack(function (stdout, stderr, err) {
                    let match = stdout.match(/Stack\s'CGPProjStk\S*'\supdate\scomplete/i)
                    // is there a project stack created?
                    if (match) {
                        writeBootstrapAndStart(cb)
                    } else {
                        log(colors.red("The attempt to automatically create a Cloud Gem Portal development project stack failed.  Please review the logs above."))
                        log(colors.red("Verify you have the Cloud Gem Framework enabled for your project.  ") + colors.yellow("\n\tRun the Project Configurator: " + enginePath() + '[path to bin folder]' + path.sep + "ProjectConfigurator.exe"))
                        log(colors.red("Verify you have default AWS credentials set with command: ") + colors.yellow("\n\tcd " + enginePath() + "\n\tlmbr_aws profile list"))
                        log(colors.red("Verify you have sufficient AWS resource capacity.  Ie. You can create more DynamoDb instances."))
                        cb(err);
                    }
                })
            }
        });
    }
}

// This is the default task - which is run when `gulp` is run
gulp.task('serve-watch', gulp.series('set:test-define-off', 'npm:link', 'set:dev-define', 'sass', 'gem-ts', 'gem-watch', initializeBootstrap, function (cb) {
    gulp.watch(["styles/**/*.scss", "app/**/*.scss"], {
        depth: 10,
        delay: 100,
        followSymlinks: true,
        ignoreInitial: false
    }, gulp.series('sass'))
        .on('change', function (path, stats) {
            log('[watcher] File ' + path.replace(/.*(?=scss)/, '') + ' was changed, compiling...')
        })
    cb()
}));

gulp.task("serve", gulp.series('set:test-define-off', 'npm:link', 'set:dev-define', 'sass', 'gem-ts', initializeBootstrap))

var launchSite = function () {
    return gulp.src('./index.html')
        .pipe(open({uri: 'http://localhost:3000'}));
}

gulp.task('default', gulp.series('serve', launchSite))
gulp.task('open', gulp.series('default'))
gulp.task('launch', gulp.series('default'))
gulp.task('start', gulp.series('default'))

// Inline app HTML and CSS files into JavaScript ES5/ES6 and TypeScript files for distribution via SystemJs
gulp.task('inline-template-app', gulp.series('sass', function (cb) {
    return gulp.src('app/**/*.ts')
        .pipe(inlineNg2Template({UseRelativePaths: true, indent: 0, removeLineBreaks: true,}))
        .pipe(tsProject())
        .pipe(gulp.dest('dist/app'))

}));

// Inline gem HTML and CSS files into JavaScript ES5/ES6 and TypeScript files for distribution via SystemJs
gulp.task('inline-template-gem', gulp.series('sass', function () {
    return gulp.src(paths.gems.files)
        .pipe(inlineNg2Template({UseRelativePaths: true, indent: 0, removeLineBreaks: true,}))
        .pipe(tsProject())
        .pipe(gulp.dest('dist/external'))
}));

var writeIndexDist = function (cb) {
    var systemjs_config_file_content = fs.readFileSync(systemjs_config_dev, 'utf-8').toString();
    var regex_config_js = /System\.config\(([\s\S]*?)\);/gm
    var match = regex_config_js.exec(systemjs_config_file_content)
    var match_group = match[1]
    var regex_unquoted_attr = / {2}([a-zA-Z].*):/gm

    // replace all non-quoted attributes with their quoted equivalents so we can parse this as JSON
    let unquoted_attr;
    while (unquoted_attr = regex_unquoted_attr.exec(match_group)) {
        match_group = match_group.replace(unquoted_attr[1] + ":", "\"" + unquoted_attr[1] + "\":")
    }
    match_group = match_group.replace(/\r\n|\r|\n/gm, '')

    // parse the object into a javascript object
    var config = JSON.parse(match_group)

    // append the required distro alterations
    config.map["app"] = "./dist/app"
    config.defaultJSExtensions = true
    config.packages = {
        "app": {
            "defaultExtension": "js",
            "main": "./main.js"
        }
    }
    var stringified_config = JSON.stringify(config)
    // get the dev index
    var index_file_content = fs.readFileSync(index, 'utf-8').toString();
    // write the empty bootstrap
    index_file_content = index_file_content.replace(/<script id=['|"]bootstrap['|"]>([\s\S])*?<\/script>/i, "<script>var bootstrap= {}</script>")
    // empty the domain
    index_file_content = index_file_content.replace(domain_pattern, "var domain = ''")
    // empty the port
    index_file_content = index_file_content.replace(port_pattern, "var port = ''")
    // set the https schema
    index_file_content = index_file_content.replace(schema_pattern, "var schema = 'https://'")
    // replace the distro config.js with an updated version from the dev config.js
    index_file_content = index_file_content.replace(/<script src=['|"]config.js['|"]>([\s\S])*?<\/script>/i, "<script>System.config(" + stringified_config + ");</script>")
    // write systemjs load script
    var socketio_loader = 'let normalizeFn = System.normalize; \n' +
        '\tlet customNormalize = function (name, parentName) { \n' +
        '\t\tif ((name[0] != \'.\' || (!!name[1] && name[1] != \'/\' && name[1] != \'.\')) && name[0] != \'/\' && !name.match(System.absURLRegEx)) \n' +
        '\t\t\treturn normalizeFn(name, parentName) \n' +
        '\t\treturn name \n' +
        '\t} \n' +
        '\tSystem.normalize = customNormalize; \n' +
        '\tSystem.import(dependencies).then(function () { \n' +
        '\t\tSystem.normalize = customNormalize; \n ' +
        '\t\tSystem.import(app).then(function () { \n' +
        '\t\tSystem.import(\'app\'); \n' +
        '\t\t}); \n' +
        '\t\tSystem.normalize = normalizeFn; \n' +
        '\t}); \n' +
        '\tSystem.normalize = normalizeFn; \n'
    writeFile(index_dist, index_file_content.replace(/<script id=['|"]socket.io_load['|"]>([\s\S])*?<\/script>/i, "<script>" + socketio_loader + "</script>"))

    // replace the distro config.js with an updated version from the dev config.js
    // gulp uses this file to transpile ts to js.
    writeFile(systemjs_config, "System.config(" + stringified_config + ");")
    cb()
}

gulp.task('js', gulp.series(writeIndexDist))

gulp.task('bundle-gems', gulp.series('js', 'inline-template-gem', 'gem-ts', function (cb) {
    var builder = new Builder('', systemjs_config);
    return cloudGems()
        .pipe(through2.obj(function bundle(file, encoding, done) {
            var gem_path = file.path.substring(0, file.path.lastIndexOf(path.sep) + 1)
            var names = packageName(file.path)
            var package_name = names[0]
            var gem_name = names[1]
            var source = path.join('dist/external', gem_name, "/**/*.js");
            var destination = path.join(gem_path, "/../dist/", gem_name.toLowerCase() + ".js");
            log(colors.yellow("GEM BUNDLING"), "for", gem_name)
            del(destination, {force: true}).then(function (paths) {
                log('Deleted path ', paths.join('\n'));
            });
            return builder.buildStatic('[' + source + ']', destination, {
                minify: true,
                uglify: {
                    beautify: {
                        comments: require('uglify-save-license')
                    }
                }
            }).then(function () {
                log("Bundled gem '" + gem_name + "' and copied to " + destination);
                done()
            })
        }));
}))

// Copy static assets - i.e. non TypeScript compiled source
gulp.task('copy:assets', function (cb) {
    gulp.src(['./bundles/**/*',], {base: './'})
        .pipe(gulp.dest(dist_path))

    gulp.src([index_dist], {base: './'})
        .pipe(rename('index.html'))
        .pipe(gulp.dest(dist_path))

    cb()
});


var bundleApp = function () {
    // optional constructor options
    // sets the baseURL and loads the configuration file
    var builder = new Builder('', systemjs_config);
    builder.loader.defaultJSExtensions = true;
    return builder.bundle('[dist/app/**/*.js]', app_bundle, {
        minify: false,
        uglify: {
            beautify: {
                comments: require('uglify-save-license')
            }
        }
    }).then(function () {
        log('Build complete');
    }).catch(function (err) {
        log(colors.red('Build error'));
        log(err);
    });
};

var bundleDependencies = function () {
    // optional constructor options
    // sets the baseURL and loads the configuration file
    //  sourceMaps: true,
    //  lowResSourceMaps: true,
    var builder = new Builder('', systemjs_config);
    return builder.bundle('dist/**/* - [dist/**/*]', app_bundle_dependencies, {
        minify: true,
        uglify: {
            beautify: {
                comments: require('uglify-save-license')
            }
        }
    }).then(function () {
        log('Build complete');
    }).catch(function (err) {
        log('Build error');
        log.error(err);
    });
};

gulp.task('build-deploy',
    gulp.series(gulp.parallel('clean:www', 'clean:dist', 'clean:assets'),
        gulp.parallel(gulp.series('set:test-define-off', 'set:prod-define'), writeIndexDist),
        'npm:link',
        'inline-template-app',
        'bundle-gems',
        gulp.parallel(bundleDependencies, bundleApp),
        gulp.parallel('copy:assets', 'set:dev-define'))
);

/*
*   E2E TESTING TASKS
*
*/

gulp.task('test:config-path-for-drivers', function (done) {
    // add the drivers path to the shell path environment variable
    // this is need for firefox and edge
    const cwd = process.cwd();
    let driver = path.join(cwd, "e2e", "driver")
    console.log(driver)
    process.env.PATH = process.env.PATH + ';' + driver
    done()
});

gulp.task('test:webdriver-update', webdriver_update({
    webdriverManagerArgs: ['--ignore_ssl']
}));

gulp.task('test:webdriver-standalone', webdriver_standalone);

gulp.task('test:webdriver', gulp.series('test:config-path-for-drivers', 'serve', 'test:webdriver-update', 'set:test-define-on', function (cb) {
    cb()
}));

/**
 * Ensure that a CloudGemPortal User name has been provided to login into the portal.
 * Can provide via --user parameter. If omitted, defaults to 'administrator'
 */
const ensureAdminUsername = function() {
    if (argv.user === undefined) {
        log.warn("No user argument provided. Defaulting to :" + test_constants.default_administrator_user)
        username = test_constants.default_administrator_user
    }
    else {
        username = argv.user
    }
    return username;
}

/**
 * Ensure that a CloudGemPortal User password has been provided to login into the portal.
 * Can provide via --password parameter. If omitted, throws PluginError
 */
const ensureAdminPassword = function() {
    if (argv.password === undefined) {
        throw new PluginError("message-of-the-day", "argument --password must be provided")
    }
    else {
        password = argv.password;
    }
    return password;
}

const testIntegrationWelcomeModal = function (cb, index_page) {
    username = undefined
    password = undefined

    return runTest({specs: './e2e/sequential/authentication-welcome-page.e2e-spec.js'},
        [{pattern: /"firstTimeUse": false/g, value: '"firstTimeUse": true'}], cb, username, password, index_page)
};

const testIntegrationUserAdministrator = function (cb) {
    username = ensureAdminUsername();
    password = ensureAdminPassword();

    return runTest({specs: './e2e/user-administration.e2e-spec.js'},
        [{pattern: /"firstTimeUse": true/g, value: '"firstTimeUse": false'}], cb, username, password);
};

const testIntegrationNoBootstrap = function (cb, index_page) {
    username = undefined
    password = undefined

    return runTest({specs: './e2e/sequential/authentication-no-bootstrap.e2e-spec.js'},
        [{pattern: /var bootstrap = {.*}/g, value: 'var bootstrap = {}'}], cb, username, password, index_page);
};

const testIntegrationMessageOfTheDay = function (cb) {
    username = ensureAdminUsername();
    password = ensureAdminPassword();

    log("Using user:" + username + ' password:' + password)
    return runTest({specs: './e2e/message-of-the-day.e2e-spec.js'},
        [{pattern: /"firstTimeUse": true/g, value: '"firstTimeUse": false'}], cb, username, password);
};

var testIntegrationDynamicContent = function (cb) {
    username = ensureAdminUsername();
    password = ensureAdminPassword();

    return runTest({specs: './e2e/dynamic-content.e2e-spec.js'},
        [{pattern: /"firstTimeUse": true/g, value: '"firstTimeUse": false'}], cb, username, password);
};


var testAllParallel = function (cb, username, password) {
    return runTest({},
        [{pattern: /"firstTimeUse": true/g, value: '"firstTimeUse": false'}], cb, username, password);
};


const killLocalHost = function (cb) {
    // Killing node should kill any stuck gulp serve processes
    exec_process('Taskkill /IM node.exe /F', {stdio: 'inherit', timeout: 60}, (err, stdout, stderr) => {
        if (stdout) log(stdout)
    });
    exec_process('Taskkill /IM geckodriver.exe /F', {stdio: 'inherit', timeout: 60}, (err, stdout, stderr) => {
        if (stdout) log(stdout)
    });
    exec_process('Taskkill /IM MicrosoftWebDriver.exe /F', {stdio: 'inherit', timeout: 60}, (err, stdout, stderr) => {
        if (stdout) log(stdout)
    });
    exec_process('Taskkill /IM chromedriver_2.32.exe /F', {stdio: 'inherit', timeout: 60}, (err, stdout, stderr) => {
        if (stdout) log(stdout)
    });
    exec_process('Taskkill /IM chromedriver_2.33.exe /F', {stdio: 'inherit', timeout: 60}, (err, stdout, stderr) => {
        if (stdout) log(stdout)
    });

    // Final check to see if a serve process is left stuck anywhere
    log('Looking any remaining processes running on server port: ' + localhost_port)
    find('port', localhost_port)
        .then(function (data) {
            log('Found ' + data)
            if (data.length) {
                log('Process %s is listening on server port: ' + localhost_port + '. Attempting to stop', data[0].name);
                try {
                    process.kill(data[0].pid)
                } catch (err) {
                    log.error(err)
                } finally {
                    cb()
                }
            }
        }, (err) => {
            log(err);
        });
    cb()
}

const createTestRegion = function (gulp_callback, region, out) {
    createProjectStack(function (stdout, stderr, err, username, password) {
        var match = stdout.match(/Stack\s'CGPProjStk\S*'\supdate\scomplete/i)
        // is there a project stack created?
        if (match) {
            //create a deployment
            out.username = username
            out.password = password
            //createDevelopmentStack(function (stdout, stderr, err) {
            //    if (stderr) {
            //        writeBootstrapAndStart(gulp_callback)
            //    }
            gulp_callback();
            //})
        } else {
            log(colors.red("The attempt to automatically create a Cloud Gem Portal development project stack failed.  Please review the logs above."))
            log(colors.red("Verify you have the Cloud Gem Framework enabled for your project.  ") + colors.yellow("\n\tRun the Project Configurator: " + enginePath() + "[path to bin folder]" + path.sep + "ProjectConfigurator.exe"))
            log(colors.red("Verify you have default AWS credentials set with command: ") + colors.yellow("\n\tcd " + enginePath() + "\n\tlmbr_aws profile list"))
            log(colors.red("Verify you have sufficient AWS resource capacity.  Ie. You can create more DynamoDb instances."))
            gulp_callback(err);
        }
    }, region)
}

gulp.task('test:kill-local-host', killLocalHost)

gulp.task('test:integration:message-of-the-day', gulp.series('test:webdriver', testIntegrationMessageOfTheDay, killLocalHost));

gulp.task('test:integration:authentication-no-bootstrap', gulp.series('test:webdriver', testIntegrationNoBootstrap, killLocalHost));

gulp.task('test:integration:authentication-welcome-page', gulp.series('test:webdriver', testIntegrationWelcomeModal, killLocalHost));

gulp.task('test:integration:dynamic-content', gulp.series('test:webdriver', testIntegrationDynamicContent, killLocalHost));

gulp.task('test:integration:user-administrator', gulp.series('test:webdriver', testIntegrationUserAdministrator, killLocalHost));

gulp.task('test:integration:runall', gulp.series('test:webdriver', function Integration(cb) {
    gulp.series(
        testIntegrationNoBootstrap,
        testIntegrationWelcomeModal,
        gulp.parallel(
            testIntegrationDynamicContent,
            testIntegrationMessageOfTheDay
        ),
        testIntegrationUserAdministrator,
        killLocalHost,
        cb)()
}));

// sleep time expects milliseconds
function sleep(time) {
    return new Promise(function (resolve) {
        setTimeout(resolve, time)
    });
}

gulp.task('test:integration:runallregions', gulp.series(function createRegion(region_all_cb) {
    var bootstrap_file = fs.readFileSync(index, 'utf-8').toString();
    //var bootstrap_information = bootstrap_file.match(bootstrap_pattern)
    //log(colors.yellow("The original bootstrap is \n" + bootstrap_information))
    var regions = ['us-east-1']
    //var bootstrap = bootstrap_information ?  JSON.parse(bootstrap_information) : {}
    readLocalProjectSettingsPath(function (settings_path) {
        var region_creation = function (cb) {
            cb()
        };
        var project_settings_path = settings_path
        var project_settings = fs.readFileSync(settings_path, 'utf-8').toString();
        let project_settings_obj = JSON.parse(project_settings)
        log("Here is your local-project-settings.json file in case something goes wrong.")
        log(colors.green(project_settings))
        var filename = project_settings_path + "_bak"
        log(colors.yellow("Saving the local_project_settings.json as " + filename))
        writeFile(filename, project_settings)

        for (let i = 0; i < regions.length; i++) {
            let region = regions[i]
            let multiplier = i
            console.log(region)
            region_creation = gulp.parallel(region_creation,
                gulp.series(
                    function createRegionProjectStack(cb1) {
                        var project_region = undefined
                        if (project_settings_obj[region] && project_settings_obj[region].ProjectStackId) {
                            var parts = project_settings_obj[region].ProjectStackId.split(":")
                            project_region = parts[3]
                        }
                        log("Region to test: \t\t\t " + region)
                        log("Project settings region object: \t " + JSON.stringify(project_settings_obj[region]))
                        if (project_region === undefined || project_region !== region) {
                            //create the test region, we offset the starts to avoid collisions in the CGF as it was not designed to create project stacks in parallel
                            project_settings_obj[region] = {}
                            sleep(multiplier * 30000).then(function () {
                                createTestRegion(function () {
                                    log(colors.yellow("Project stack out parameters"))
                                    log(project_settings_obj[region])
                                    cb1()
                                }, region, project_settings_obj[region])
                            })
                        } else {
                            cb1()
                        }
                    },
                    function createRegionIndex(cb1) {
                        log("Region to test: \t\t\t " + region)
                        var filename = region + "_" + index
                        project_settings_obj[region].url = filename
                        log(colors.yellow("Saving the index.html with region '" + region + "' as " + filename))
                        log(colors.yellow("Project stack out parameters"))
                        log(project_settings_obj)
                        writeBootstrap(cb1, filename, region)
                    },
                    'test:webdriver',
                    //function (cb1) {
                    //    testIntegrationNoBootstrap(cb1, project_settings_obj[region].username, project_settings_obj[region].password, project_settings_obj[region].url )
                    //},
                    function (cb1) {
                        testIntegrationWelcomeModal(cb1, project_settings_obj[region].username, project_settings_obj[region].password, project_settings_obj[region].url)
                    },
                    //gulp.parallel(
                    //        testIntegrationUserAdministrator,
                    //        testIntegrationMessageOfTheDay
                    //    ),
                    function revertBootstrapAndProjectSettings(cb3) {
                        writeFile(index, bootstrap_file)
                        writeFile(project_settings_path, project_settings)
                        cb3()
                    }
                    //function deleteDeploymentStk(cb4) {
                    //    deleteDeploymentStack(cb4)
                    //},
                    //deleteProjectStack,
                )
            )
        }
        console.log("Done...main event now")
        //create all the regions in parallel
        gulp.series(
            region_creation,
            killLocalHost,
            function (main_callback) {
                main_callback()
                region_all_cb()
            })()
    })


}));

/*
 * Generate an ID to append to user names so multiple browsers running in parallel don't use the same user name.
*/
var makeId = function () {
    var text = "";
    var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (var i = 0; i < 5; i++)
        text += possible.charAt(Math.floor(Math.random() * possible.length));

    return text;
}

var runTestExit = function (original_file_content) {
    gulp.series('set:test-define-off')()
    writeFile(index, original_file_content);
}

var runTest = function (context, bootstrap_setting, cb, username, password, index_page, onerr) {
    log(process.cwd())
    var protractor_args = [
        '--params.user.username', "zzztestUser1" + makeId(),
        '--params.user.email', "cgp-integ-test" + makeId(),
        '--params.user.password', "Test01)!",
        '--params.user.newPassword', "Test02)@",
        '--params.admin.username', username,
        '--params.admin.password', password,
        '--params.url', index_page ? "http://localhost:3000/" + index_page : "http://localhost:3000/index.html"
    ]
    var source_file = "./e2e/*.e2e-spec.js"

    var original_file_content = fs.readFileSync(index_page ? index_page : index, 'utf-8').toString();

    if (bootstrap_setting) {
        for (var i = 0; i < bootstrap_setting.length; i++) {
            var setting = bootstrap_setting[i]
            log(colors.yellow(JSON.stringify(setting)))
            replaceFileContent(index_page ? index_page : index, setting.pattern, setting.value)
        }
    }

    if (context && context.specs) {
        source_file = context.specs
    }
    log("Running test with args: ")
    log("\t\tSource: \t\t" + source_file)
    log("\t\tArgs: \t\t\t\t" + JSON.stringify(protractor_args))
    log("\t\tWorking Directory: \t" + process.cwd())
    log("\t\tBootstrap: \t\t" + fs.readFileSync(index_page ? index_page : index, 'utf-8').toString().match(bootstrap_pattern))
    log("\t\Protractor Config Path: \t" + './e2e/protractor.config.js')

    return gulp.src([source_file])
        .pipe(protractor({
            configFile: './e2e/protractor.config.js',
            args: protractor_args
        }))
        .on('error', function (e) {
            log(new PluginError('GULP', e.message, {showStack: true}));
            if (onerr)
                onerr();
            runTestExit(original_file_content);
            if (cb)
                cb()

        })
        .on('end', function () {
            runTestExit(original_file_content);
            log("The test '" + source_file + "' is done.");
            if (cb)
                cb()
        })
}
/*
*  END TESTING
*/

gulp.task('package:install', function (cb) {
    if (Object.keys(argv).length <= 2) {
        log("")
        log("usage: package:install --[COMMAND]=[value]")
        log("Description:\t\t\tUsed to install a new package or install the currently defined packages.")
        log("")
        log("COMMAND")
        log("")
        log("package:\t\t\tName of the package to install.")
        log("repos:\t\t\tName of the packages repository.  Example [npm | github]")
        log("version:\t\t\tThe version you wish to install.")
        log("devonly:\t\t\tThe package will appear in your devDependencies.")
        log("")
    }
    log("INSTALLING package(s)")
    var npmcommand = 'npm i'
    var jspmcommand = 'jspm install'
    if (argv.package && argv.repos) {
        log("\tPackage: " + argv.package)
        log("\tRepository: " + argv.repos)
        log("\tDevonly: " + argv.devonly)
        log("\tVersion: " + argv.pkgversion)
        npmcommand = 'npm i ' + argv.package
        jspmcommand = 'jspm install ' + argv.repos + ":" + argv.package

        if (argv.pkgversion) {
            jspmcommand += "@" + argv.pkgversion
            npmcommand += "@" + argv.pkgversion
        }
        npmcommand += ' --save '

        if (argv.devonly)
            npmcommand += " --save-dev"
    } else {
        throw new PluginError({
            plugin: 'package:install',
            message: 'package and repro need to be provided.'
        });
    }
    return gulp.src('./')
        .pipe(exec(npmcommand, gulp_exec.options))
        .pipe(exec.reporter(gulp_exec.reportOptions))
        .pipe(exec(jspmcommand, gulp_exec.options))
        .pipe(exec.reporter(gulp_exec.reportOptions))
})

gulp.task('package:uninstall', function () {
    if (Object.keys(argv).length <= 2) {
        log("")
        log("usage: package:uninstall --[COMMAND]=[value]")
        log("Description:\t\t\tUsed to uninstall a package.")
        log("")
        log("COMMAND")
        log("")
        log("package:\t\t\tName of the package to install.")
        log("repos:\t\t\tName of the packages repository.  Example [npm | github]")
        log("")
        return;
    }
    log("UNINSTALLING package(s)")
    if (argv.package && argv.repos) {
        log("\tPackage: " + argv.package)
        log("\tRepository: " + argv.repos)
        var npm_command = 'npm uninstall ' + argv.package + ' --save'
        var jspm_command = 'jspm uninstall ' + argv.repos + ":" + argv.package
        return gulp.src('./')
            .pipe(exec(npm_command, gulp_exec.options))
            .pipe(exec.reporter(gulp_exec.reportOptions))
            .pipe(exec(jspm_command, gulp_exec.options))
            .pipe(exec.reporter(gulp_exec.reportOptions))
    } else {
        throw new PluginError(
            "gulp",
            "No package or repos parameters were found. Command usage: gulp package:uninstall --package=packagename --repos=repository[\'npm\'|\'github\']",
            {show_stack: true})
    }
})

function globalDefines() {
    enginePath()
    process.chdir(cgp_path)
    replaceOrAppend(environment_file, /export const metricWhiteListedCloudGem = \[.*\]/g, "export const metricWhiteListedCloudGem = []")
    if (argv.builtByAmazon) {
        console.log("Whitelisting for metrics: " + getCloudGemFolderNames(true))
        replaceOrAppend(environment_file, /export const metricWhiteListedCloudGem = \[.*\]/g, "export const metricWhiteListedCloudGem = [" + getCloudGemFolderNames(true) + "]")
    }
    replaceOrAppend(environment_file, /export const metricWhiteListedFeature = \[.*\]/g, "export const metricWhiteListedFeature = ['Cloud Gems', 'Support', 'Analytics', 'Admin', 'User Administration']")
    replaceOrAppend(environment_file, /export const metricAdminIndex = 3/g, "export const metricAdminIndex = 3")
}

function packageName(gem_path) {
    var parts = gem_path.split(path.sep)
    var gem_name = parts[parts.length - 1].split('.')[0].toLowerCase()
    var package_name = package_scope_name + "/" + gem_name
    return [package_name, gem_name]
}

// Utility function to get all folders in a path
function getFolders(dir) {
    return fs.readdirSync(dir)
        .filter(function (file) {
            return fs.statSync(path.join(dir, file)).isDirectory();
        });
}

function getCloudGemFolderNames(addQuotes, asRelative) {
    var relativePathPrefix = path.join(path.join(path.join("..", ".."), ".."), "..");
    var folders = getFolders(relativePathPrefix)
    var cloudGems = []
    folders.map(function (folder) {
        if (folder.toLowerCase().startsWith("cloudgem")) {
            if (addQuotes)
                cloudGems.push("\"" + folder + "\"")
            else if (asRelative)
                cloudGems.push(path.join(relativePathPrefix, folder))
            else
                cloudGems.push(folder)
        }
    })
    return cloudGems
}


function cloudGems(addquotes, asrelative) {
    // We are looking for any gem with a node project.
    // There could potentially be other Cloud Gems without node projects but they are not loaded by the CGP.
    let searchPath = paths.gems.relativeToGemsFolder + path.sep + "**" + path.sep + "*.njsproj"
    return gulp.src([searchPath, '!*CloudGemFramework/**/*', '!*node_modules/**/*', '!../../**/*'], {base: "."})
}

function writeFile(filepath, content) {
    filepath = path.resolve(process.cwd(), filepath)
    mkdirp.sync(getDirName(filepath))
    return fs.writeFileSync(filepath, content, function (err) {
        if (err) {
            log("ERROR::writeFile::" + err)
        }
    });
}

function readContent(filepath) {
    filepath = path.resolve(process.cwd(), filepath)
    let file_content = ''
    try {
        file_content = fs.readFileSync(filepath, 'utf-8').toString();
    } catch (err) {
        if (err.code === 'ENOENT') {
            log('File not found:' + path);
        } else {
            throw err;
        }
    }
    return file_content
}

function replaceOrAppend(filepath, regex, content) {
    let file_content = readContent(filepath)
    let match = file_content.match(regex)
    if (match) {
        file_content = file_content.replace(new RegExp(regex, 'g'), content)
    } else {
        file_content = file_content + '\r\n' + content
    }
    writeFile(filepath, file_content)
}

function append(filepath, regex, content) {
    let file_content = readContent(filepath)
    let match = file_content.match(regex)
    if (!match) {
        file_content = file_content + '\r\n' + content
        writeFile(filepath, file_content)
    }
}

function replaceFileContent(filepath, regex, content, output_path) {
    let file_content = fs.readFileSync(filepath, 'utf-8').toString();
    let match = file_content.match(regex)
    if (match) {
        let result = file_content.replace(new RegExp(regex, 'g'), content)
        writeFile(output_path ? output_path : filepath, result)
    }
}

var startServer = function () {
    let config = fs.readFileSync('server.js', 'utf-8').toString();
    let port = config.match(/server.listen(.*)/i)[0]
    log(colors.yellow("Starting the local web server."))
    log(colors.magenta("Launch your browser and enter the url http://localhost:" + port.replace('server.listen(', '').replace(')', '')))
    // Start a server
    nodemon({
        // the script to run the app
        script: 'server.js',
        // this listens to changes in any of these files/routes and restarts the application
        watch: ["server.js"]
        // Below i'm using es6 arrow functions but you can remove the arrow and have it a normal .on('restart', function() { // then place your stuff in here }
    }).on('restart', function () {
        gulp.src('server.js')
    });
}

var writeBootstrapAndStart = function (cb) {
    log(colors.yellow(`A project stack exists.  Creating the local bootstrap at path ${cgp_path + path.sep + index} .`))
    writeBootstrap(function (err) {
        startServer()
        cb(err)
    })
}

var writeBootstrap = function (cb, output_path, region) {
    return executeLmbrAwsCommand('lmbr_aws cloud-gem-framework cloud-gem-portal --show-bootstrap-configuration' + (region ? ' --region-override ' + region : ''), function (stdout, stderr, err) {
        var bootstrap_information = stdout.match(bootstrap_pattern)
        //is there a bootstrap configuration present?
        if (bootstrap_information) {
            replaceFileContent(index, /var\s*bootstrap\s*=\s*{.*}/i, "var bootstrap = " + bootstrap_information[0], output_path)
        } else {
            log(colors.red("The attempt to automatically write the Cloud Gem Portal development bootstrap failed.  Please review the logs above."))
        }
        cb(err)
    })
}

var cgp_path = undefined
var enginePath = function () {
    if (!cgp_path) {
        cgp_path = process.cwd()
    }
    return cgp_path + path.sep + ".." + path.sep + ".." + path.sep + ".." + path.sep + ".." + path.sep + ".." + path.sep
}

var createDevelopmentStack = function (handler_callback, name) {
    if (!name)
        name = deploymentStackName()
    log(colors.yellow("Attempting to create a deployment stack. Please wait...this could take up to 15 minutes."))
    executeLmbrAwsCommand('lmbr_aws deployment create  --confirm-aws-usage --confirm-security-change --deployment ' + name, function (stdout, stderr, err) {
        handler_callback(stdout, stderr, err)
    })
}

var createProjectStack = function (handler_callback, region) {
    if (!region)
        region = 'us-east-1'
    var stackName = projectStackName()
    log(colors.yellow("Attempting to create a project stack with name '" + stackName + "' and region '" + region + "'. Please wait...this could take up to 10 minutes."))
    executeLmbrAwsCommand('lmbr_aws project create --stack-name ' + stackName + ' --confirm-aws-usage --confirm-security-change --region ' + region, function (stdout, stderr, err) {
        var username = stdout.match(/Username:\s(\S*)/i)
        var password = stdout.match(/Password:\s(\S*)/i)
        if (username) {
            log(colors.blue("Your administrator account credentials are:"))
            log(colors.blue(username[0]))
            log(colors.blue(password[0]))
        }
        handler_callback(stdout, stderr, err, username, password)
    })
}

var deleteProjectStack = function (gulp_callback) {
    log(colors.yellow("Attempting to delete a project stack. Please wait...this could take up to 5 minutes."))
    executeLmbrAwsCommand('lmbr_aws project delete -D', function (response) {
        gulp_callback()
    })
}

var deleteDeploymentStack = function (handler_callback, stackname) {
    if (!stackname)
        stackname = 'Stack1'
    log(colors.yellow("Attempting to delete a deployment stack. Please wait...this could take up to 10 minutes."))
    executeLmbrAwsCommand('lmbr_aws deployment delete -d ' + stackname, function (response) {
        handler_callback()
    })
}

var readLocalProjectSettingsPath = function (handler_callback) {
    log(colors.yellow("Reading your current local_project_settings.json file."))
    executeLmbrAwsCommand('lmbr_aws cloud-gem-framework paths', function (response) {
        const regex = /Local Project Settings\s*(\S*)\s*/i
        const match = regex.exec(response)
        let path = match[1]
        log(colors.yellow("Found at location '" + path + "'"))
        handler_callback(path)
    })
}

var deploymentStackName = function () {
    return 'CGPDepStk' + Date.now().toString().substring(5, Date.now().length)
}

var projectStackName = function () {
    return 'CGPProjStk' + Date.now().toString().substring(5, Date.now().length)
}

var executeLmbrAwsCommand = function (command, callback) {
    log("changing to " + enginePath() + " from " + process.cwd())
    process.chdir(enginePath())
    log(colors.yellow(command))
    exec_process(command, function (err, stdout, stderr) {
        log(colors.red("changing back to " + cgp_path))
        process.chdir(cgp_path)
        log(stdout);
        log(colors.red(stderr));
        callback(stdout, stderr, err)
    });
}

