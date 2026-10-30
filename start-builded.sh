#!/bin/bash

cd "/app/build/service-executables"
"./planner-gateway" &
"./planner-users" &
"./planner-frontend" &
"./planner-calendars" &
wait
echo "all services exited"
