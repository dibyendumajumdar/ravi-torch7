# How to run the tests


### On Windows

```
ravi -ltorch -e "t=torch.test(); if t.errors[1] then os.exit(1) end"
ravi -ltorch test_qr.lua
ravi -ltorch test_aliasMultinomial.lua
ravi -ltorch test_half.lua
ravi test_writeObject.lua
ravi test_Tester.lua
```

