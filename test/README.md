# How to run the tests


### On Windows

```
ravi -ltorch -e "t=torch.test(); if t.errors[1] then os.exit(1) end"
ravi test_writeObject.lua
ravi test_Tester.lua
```
