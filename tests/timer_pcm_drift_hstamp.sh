#!/bin/bash

# echo "Running: 1 0 "
# ./build/tests/test_timer_pcm_drift_hstamp 1 0 >outputs/test_timer_pcm_drift_hstamp_1_0.csv
# echo "Running: 2 0 "
# ./build/tests/test_timer_pcm_drift_hstamp 2 0 >outputs/test_timer_pcm_drift_hstamp_2_0.csv
# echo "Running: 3 0 "
# ./build/tests/test_timer_pcm_drift_hstamp 3 0 >outputs/test_timer_pcm_drift_hstamp_3_0.csv
# echo "Running: 4 0 "
# ./build/tests/test_timer_pcm_drift_hstamp 4 0 >outputs/test_timer_pcm_drift_hstamp_4_0.csv
# echo "Running: 5 0 "
# ./build/tests/test_timer_pcm_drift_hstamp 5 0 >outputs/test_timer_pcm_drift_hstamp_5_0.csv
# echo "Running: 1 1 "
# ./build/tests/test_timer_pcm_drift_hstamp 1 1 >outputs/test_timer_pcm_drift_hstamp_1_1.csv
# echo "Running: 2 1 "
# ./build/tests/test_timer_pcm_drift_hstamp 2 1 >outputs/test_timer_pcm_drift_hstamp_2_1.csv
# echo "Running: 3 1 "
# ./build/tests/test_timer_pcm_drift_hstamp 3 1 >outputs/test_timer_pcm_drift_hstamp_3_1.csv
# echo "Running: 4 1 "
# ./build/tests/test_timer_pcm_drift_hstamp 4 1 >outputs/test_timer_pcm_drift_hstamp_4_1.csv
# echo "Running: 5 1 "
# ./build/tests/test_timer_pcm_drift_hstamp 5 1 >outputs/test_timer_pcm_drift_hstamp_5_1.csv

autoplot outputs/test_timer_pcm_drift_hstamp_1_0.csv
autoplot outputs/test_timer_pcm_drift_hstamp_2_0.csv
autoplot outputs/test_timer_pcm_drift_hstamp_3_0.csv
autoplot outputs/test_timer_pcm_drift_hstamp_4_0.csv
autoplot outputs/test_timer_pcm_drift_hstamp_5_0.csv
autoplot outputs/test_timer_pcm_drift_hstamp_1_1.csv
autoplot outputs/test_timer_pcm_drift_hstamp_2_1.csv
autoplot outputs/test_timer_pcm_drift_hstamp_3_1.csv
autoplot outputs/test_timer_pcm_drift_hstamp_4_1.csv
autoplot outputs/test_timer_pcm_drift_hstamp_5_1.csv
