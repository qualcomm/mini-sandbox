# Libminisandbox

## How to add libminisandbox to a generic Python project

```
cp -r ./mini_sandbox/src/py/pybind_bindigs/ ./
```
### libminitapbox 

```
import os
import sys
sys.path.append("pybind_bindings/")
import pyminitapbox as mn_sbx

```

### libminisandbox
```
import os
import sys
sys.path.append("pybind_bindings/")
import pyminisandbox as mn_sbx
```

### Run with default configurations

```
mn_sbx.mini_sandbox_setup_default()
mn_sbx.mini_sandbox_start()
```
## Compile libminisanbdox 

```
./build.sh
```
