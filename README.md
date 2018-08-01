# Datacenter Transport Protocols with Worst-Case Flow Completion Guarantees

This repository contains the code to reproduce the experimental results in "Datacenter Transport Protocols with Worst-Case Flow Completion Guarantees".

Evaluations are based on the YAPS simulator (https://github.com/NetSys/simulator).

## Run and compile: 

`CXX=g++ cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-std=c++14"` then `make`. To simulate all algorithms copy executable `simulator` to `runner/` and run `python run_experiments.py`.

The results will be stored in files of the form: `result_[name of algorithm]_split=[split value]_compres=[compress_value]_mar=[mar_value].txt`

* `split_value` - denotes how sparse the flows are, corresponds to parameter alpha.
* `compress_value` - a multiplier for flow start time, corresponds to parameter beta.
* `mar` - denotes an approach used for multiple input-output switch: mar = 0 -- no blocking, mar = 1 -- marriage, mar = 2 -- Hungary.



