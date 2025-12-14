@REM
@REM Copyright 2010 the original author or authors.
@REM
@REM Licensed under the Apache License, Version 2.0 (the "License");
@REM you may not use this file except in compliance with the License.
@REM You may obtain a copy of the License at
@REM
@REM      http://www.apache.org/licenses/LICENSE-2.0
@REM
@REM Unless required by applicable law or agreed to in writing, software
@REM distributed under the License is distributed on an "AS IS" BASIS,
@REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@REM See the License for the specific language governing permissions and
@REM limitations under the License.
@REM

@REM Set local scope for the variables with windows NT shell
@REM See http://ss64.com/nt/setlocal.html
SETLOCAL

@REM Add default JVM options here. You can also use JAVA_OPTS and GRADLE_OPTS system environment variables.
SET DEFAULT_JVM_OPTS=

:init
@REM Decide whether to use bash or cmd.exe based on whether we are in a Cygwin/Msys environment.
@REM And whether to use find.exe or findstr.exe
IF DEFINED MSYSTEM (
    SET CYGWIN=true
    SET FIND=find
) ELSE (
    SET CYGWIN=false
    SET FIND=findstr
)

@REM Find the project root directory, which is the parent of this script's directory.
@REM It's possible that this script is called from anywhere, so we need to find the actual location.
SET SCRIPT_DIR=%~dp0
FOR %%i IN ("%SCRIPT_DIR%") DO SET APP_HOME=%%~dpi

@REM Check if the Java executable exists
IF NOT DEFINED JAVA_HOME (
    FOR %%i IN ("%JAVA_HOME%\bin\java.exe") DO (
        IF EXIST %%i (
            SET JAVA_EXE="%%i"
        )
    )
)
IF NOT DEFINED JAVA_EXE (
    FOR %%i IN ("java.exe") DO (
        IF EXIST %%~dp$PATH:i (
            SET JAVA_EXE=%%~dp$PATH:i
        )
    )
)
IF NOT DEFINED JAVA_EXE (
    ECHO ERROR: JAVA_HOME is not set and no 'java.exe' command can be found in your PATH.
    ECHO Please set the JAVA_HOME variable in your environment to match the
    ECHO location of your Java installation or add 'java.exe' to your PATH.
    EXIT /B 1
)

@REM Determine the class path for the wrapper
SET APP_CLASSPATH=%APP_HOME%gradle\wrapper\gradle-wrapper.jar

@REM Execute Gradle.
"%JAVA_EXE%" %DEFAULT_JVM_OPTS% %JAVA_OPTS% %GRADLE_OPTS% %GRADLE_JVM_ARGS% ^
    -cp "%APP_CLASSPATH%" org.gradle.wrapper.GradleWrapperMain %*

:end
@REM End local scope for the variables.
ENDLOCAL