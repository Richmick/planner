> [!NOTE]
> You need to install drogon_ctl (view [drogon installation guide](https://github.com/drogonframework/drogon/wiki/ENG-02-Installation "drogon wiki::ENG::2_instalation")) and add it to path (or replace executable paths to yours)

# Project
To create project run drogon_ctl tool. It will create a folder with new project template
```shell
drogon_ctl create project <your_project_name>
```
On Windows with Conan manager you need to create .sln project from cmake:
```shell
cd build
conan install .. -s compiler="msvc" -s compiler.version=194  -s compiler.cppstd=23 -s build_type=Debug  --output-folder . --build=missing
cmake .. --preset conan-default -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_PREFIX_PATH=<path-to-drogon-built-result>
```
Then you can build your project with Visual Studio via sln and rerun cmake to update project

[see more](https://github.com/drogonframework/drogon/wiki/ENG-03-Quick-Start "drogon wiki::ENG::3_Quick-Start") for project setup

# CLI
Drogon_ctl allows to create many items with templates ([see for more details](https://github.com/drogonframework/drogon/wiki/ENG-12-drogon_ctl-Command "drogon wiki::ENG::12_drogon_ctl-Command")):
```shell
$ dg_ctl help create
Use create command to create some source files of drogon webapp

Usage:drogon_ctl create <view|controller|filter|project|model> [-options] <object name>

drogon_ctl create view <csp file name> [-o <output path>] [-n <namespace>]|[--path-to-namespace] //create HttpView source files from csp file
drogon_ctl create controller [-s] <[namespace::]class_name> //create HttpSimpleController source files
drogon_ctl create controller -h <[namespace::]class_name> //create HttpController source files
drogon_ctl create controller -w <[namespace::]class_name> //create WebSocketController source files
drogon_ctl create filter <[namespace::]class_name> //create a filter named class_name
drogon_ctl create project <project_name> //create a project named project_name
drogon_ctl create model <model_path> //create model classes in model_path
```
