pipeline{
    agent {
        node {
            //cloud 'kubernetes'
            label 'openshift-autoscale'
        }
    }
    stages{
        stage('setup'){
            steps{
                echo 'Starting AutoPas Pipeline'
                githubNotify context: 'build', description: 'build pending...',  status: 'PENDING', targetUrl: currentBuild.absoluteUrl
                githubNotify context: 'test', description: 'test pending...',  status: 'PENDING', targetUrl: currentBuild.absoluteUrl
            }
        }
        stage("build") {
            steps{
                parallel(
                    "default": {
                        githubNotify context: 'build', description: 'build in progress...',  status: 'PENDING', targetUrl: currentBuild.absoluteUrl
                        container('autopas-gcc7-cmake-make') {
                            dir("build"){
                                sh "cmake .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    "gcc openmp": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-openmp"){
                                sh "cmake -DOPENMP=ON .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    "gcc openmp address-sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-openmp-address-sanitizer"){
                                sh "cmake -DOPENMP=ON -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    "address sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-addresssanitizer"){
                                sh "cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    "address sanitizer release": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-addresssanitizer-release"){
                                sh "cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_ADDRESS_SANITIZER=ON .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    "thread sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-threadsanitizer"){
                                // this is for simple testing of our threading libraries.
                                sh "cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_THREAD_SANITIZER=ON .."
                                sh "make -j 4 2>&1 > buildlog.txt || (cat buildlog.txt && exit 1)"
                            }
                        }
                    },
                    /*dir("build-memorysanitizer"){
                        sh "CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_MEMORY_SANITIZER=ON .."
                        sh "make -j 4"
                    }*/
                    "clang openmp": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-openmp"){
                                sh "CC=clang CXX=clang++ cmake -G Ninja -DOPENMP=ON .."
                                sh "ninja -j 4 2>&1 > buildlog_clang.txt || (cat buildlog_clang.txt && exit 1)"
                            }
                        }
                    },
                    "clang ninja address sanitizer": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-addresssanitizer-debug"){
                                sh "CC=clang CXX=clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON .."
                                sh "ninja -j 4 2>&1 > buildlog_clang.txt || (cat buildlog_clang.txt && exit 1)"
                            }
                        }
                    },
                    "clang ninja address sanitizer release": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-addresssanitizer-release"){
                                sh "CC=clang CXX=clang++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_ADDRESS_SANITIZER=ON .."
                                sh "ninja -j 4 2>&1 > buildlog_clang.txt || (cat buildlog_clang.txt && exit 1)"
                            }
                        }
                    },
                    "archer": {
                        container('autopas-archer'){
                            dir("build-archer"){
                                sh "CC=clang-archer CXX=clang-archer++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_VECTORIZATION=OFF .."
                                sh "ninja -j 4 2>&1 > buildlog_clang.txt || (cat buildlog_clang.txt && exit 1)"
                            }
                        }
                    },
                    "intel": {
                        container('autopas-intel18'){
                            dir("build-intel"){
                                sh "icc --version"
                                sh "icpc --version"
                                sh "CC=`which icc` CXX=`which icpc` cmake -DOPENMP=OFF .."
                                sh "make -j 4 2>&1 > buildlog_intel.txt || (cat buildlog_intel.txt && exit 1)"
                            }
                        }
                    },
                    "intel openmp": {
                        container('autopas-intel18'){
                            dir("build-intel-ninja-openmp"){
                                sh "CC=`which icc` CXX=`which icpc` cmake -G Ninja -DOPENMP=ON .."
                                sh "ninja -j 4 2>&1 > buildlog_intel.txt || (cat buildlog_intel.txt && exit 1)"
                            }
                        }
                    }
                )
            }
            post{
                always{
					warnings canComputeNew: false, categoriesPattern: '', defaultEncoding: '', excludePattern: '', healthy: '', includePattern: '', messagesPattern: '', parserConfigurations: [[parserName: 'Clang (LLVM based)', pattern: 'build*/buildlog_clang.txt'], [parserName: 'GNU Make + GNU C Compiler (gcc)', pattern: 'build*/buildlog.txt'], [parserName: 'Intel C Compiler', pattern: 'build*/buildlog_intel.txt']], unHealthy: '', unstableTotalAll: '0', unstableTotalHigh: '0', unstableTotalLow: '0', unstableTotalNormal: '0'
                }
                success{
                    githubNotify context: 'build', description: currentBuild.durationString,  status: 'SUCCESS', targetUrl: currentBuild.absoluteUrl
                }
                failure{
                    githubNotify context: 'build', description: currentBuild.description, status: 'FAILURE', targetUrl: currentBuild.absoluteUrl
                }
                unstable{
                    githubNotify context: 'build', description: currentBuild.description, status: 'FAILURE', targetUrl: currentBuild.absoluteUrl
                }
                aborted{
                    githubNotify context: 'build', description: 'build aborted',  status: 'ERROR', targetUrl: currentBuild.absoluteUrl
                }
            }
        }
        stage("test") {
            steps{
                parallel(
                    "default": {
                        githubNotify context: 'test', description: 'test in progress...',  status: 'PENDING', targetUrl: currentBuild.absoluteUrl
                        container('autopas-gcc7-cmake-make') {
                            dir("build"){
                                //sh "env CTEST_OUTPUT_ON_FAILURE=1 make test"
                                sh 'env GTEST_OUTPUT="xml:$(pwd)/test.xml" ./tests/testAutopas/runTests'
                            }
                        }
                    },
                    "gcc openmp": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-openmp"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "gcc openmp address-sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-openmp-address-sanitizer"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "address sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-addresssanitizer"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "address sanitizer release": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-addresssanitizer-release"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "thread sanitizer": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build-threadsanitizer"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    /*dir("build-memorysanitizer"){
                        sh './tests/testAutopas/runTests'
                    }*/
                    "clang openmp": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-openmp"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "clang ninja address sanitizer": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-addresssanitizer-debug"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "clang ninja address sanitizer release": {
                        container('autopas-clang6-cmake-ninja-make'){
                            dir("build-clang-ninja-addresssanitizer-release"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "archer": {
                        container('autopas-archer'){
                            dir("build-archer"){
                                sh './tests/testAutopas/runTests'
                            }
                        }
                    },
                    "checkExamples": {
                        container('autopas-gcc7-cmake-make') {
                            dir("build/examples") {
                                sh 'ctest -C checkExamples -j8'
                            }
                        }
                    }
                )
            }
            post{
                success{
                    githubNotify context: 'test', description: currentBuild.durationString,  status: 'SUCCESS', targetUrl: currentBuild.absoluteUrl
                }
                failure{
                    githubNotify context: 'test', description: currentBuild.description,  status: 'FAILURE', targetUrl: currentBuild.absoluteUrl
                }
                unstable{
                    githubNotify context: 'test', description: currentBuild.description,  status: 'FAILURE', targetUrl: currentBuild.absoluteUrl
                }
                aborted{
                    githubNotify context: 'test', description: 'build aborted',  status: 'ERROR', targetUrl: currentBuild.absoluteUrl
                }
            }
        }
        stage("build documentation"){
            steps{
                container('autopas-cmake-doxygen-make'){
                    dir("build-doxygen") {
                        sh 'cmake ..'
                        sh 'make doc_doxygen 2>DoxygenWarningLog.txt'
                    }
                }
                stash includes: 'build-doxygen/doc_doxygen/html/**', name: 'doxydocs'
            }
        }
        stage("update documentation"){
            when{ branch 'master' }
            agent{ label 'atsccs11_prio' }
            steps{
                unstash 'doxydocs'
                dir("build-doxygen"){
                    sh 'touch /import/www/wwwsccs/html/AutoPas/doxygen_doc/master || echo 0'
                    sh 'rm -rf /import/www/wwwsccs/html/AutoPas/doxygen_doc/master || echo 0'
                    sh 'cp -r doc_doxygen/html /import/www/wwwsccs/html/AutoPas/doxygen_doc/master'
                }
            }
        }
        stage("generate reports"){
            steps{
                // get test results -- mainly to get number of tests
                junit 'build/test.xml'

                // get doxygen warnings
                warnings canComputeNew: false, canResolveRelativePaths: false, categoriesPattern: '', defaultEncoding: '', excludePattern: '.*README.*', healthy: '', includePattern: '', messagesPattern: '', parserConfigurations: [[parserName: 'Doxygen', pattern: 'build-doxygen/DoxygenWarningLog.txt']], unHealthy: '', unstableTotalAll: '0'

                // generate coverage
                dir("coverage"){
                    container('autopas-build-code-coverage'){
                        sh "cmake -DCodeCoverage=ON -DCMAKE_BUILD_TYPE=Debug .."
                        sh "make AutoPas_cobertura -j 4"
                    }
                    cobertura autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: 'coverage.xml', conditionalCoverageTargets: '70, 0, 0', failUnhealthy: false, failUnstable: false, lineCoverageTargets: '80, 0, 0', maxNumberOfBuilds: 0, methodCoverageTargets: '80, 0, 0', onlyStable: false, sourceEncoding: 'ASCII', zoomCoverageChart: false
                }
            }
        }
        stage("clang format"){
            steps{
                dir("clang-format"){
                    container('autopas-clang6-cmake-ninja-make'){
                        sh "CC=clang CXX=clang++ cmake -G Ninja -DOPENMP=ON .."
                        sh "ninja clangformat"
                    }
                    script{
                        // return 2 if files have been modified by clang-format, 0 otherwise
                        try{
                            // if files were modified, return 2
                            sh "git status | grep -q modified && exit 2 || exit 0"
                        } catch (Exception e) {
                            // change detected
                            currentBuild.result = 'UNSTABLE'
                            echo 'clang format errors detected. please format the code properly. Affected files:'
                            sh "git status | grep modified"
                        }
                    }
                }
            }
        }
        stage("custom checks"){
            steps{
                dir("src"){
                    script{
                        // check if all header files have a #pragma once
                        try{
                            // if header files do not contain #pragma once, make build unstable
                            sh 'grep -L "#pragma once" -r . | grep -q "\\.h" && exit 2 || exit 0'
                        } catch (Exception e) {
                            // change detected
                            currentBuild.result = 'UNSTABLE'
                            echo 'all header include guards should be implemented using "#pragma once". Affected files:'
                            sh 'grep -L "#pragma once" -r . | grep "\\.h"'
                        }

                        // check if all files are documented with @file or \file doxygen comments
                        try{
                            // if .cpp or .h files do not contain a file comment, return 2
                            sh "grep '\\\\file\\|\\@file' -Lr . | grep -q '\\.cpp\\|\\.h' && exit 2 || exit 0"
                        } catch (Exception e) {
                            // change detected
                            currentBuild.result = 'UNSTABLE'
                            echo 'all .h and .cpp files should be documented with doxygen comments (@file)". Affected files:'
                            sh "grep '\\\\file\\|\\@file' -Lr . | grep '\\.cpp\\|\\.h'"
                        }
                    }
                }
            }
        }
    }
    post {
        changed {
            echo "change"
        }
        unstable {
            echo "unstable"
        }
        success {
            echo "success"
        }
        failure {
            echo "failure"
        }
        aborted {
            echo "aborted"
        }
    }
}
