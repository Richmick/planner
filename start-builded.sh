#!/bin/bash

set -e
cd "/app/build/service-executables"

./planner-users &
./planner-calendars &
./planner-frontend &
sleep 2
./planner-gateway &

wait
