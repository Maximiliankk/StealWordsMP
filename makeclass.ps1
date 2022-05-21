$param1=$args[0]

ni "src/$param1.cpp"

Add-Content -Path "src/$param1.cpp" -Value "#include `"$param1.h`"`n"

Add-Content -Path "src/$param1.cpp" -Value "void $param1::Init()"
Add-Content -Path "src/$param1.cpp" -Value "{"
Add-Content -Path "src/$param1.cpp" -Value "}`n"

Add-Content -Path "src/$param1.cpp" -Value "void $param1::Update()"
Add-Content -Path "src/$param1.cpp" -Value "{"
Add-Content -Path "src/$param1.cpp" -Value "}"


ni "src/$param1.h"

Add-Content -Path "src/$param1.h" -Value "#pragma once`n"

Add-Content -Path "src/$param1.h" -Value "class $param1"
Add-Content -Path "src/$param1.h" -Value "{"
Add-Content -Path "src/$param1.h" -Value "public:"
Add-Content -Path "src/$param1.h" -Value "private:"
Add-Content -Path "src/$param1.h" -Value "    void Init();"
Add-Content -Path "src/$param1.h" -Value "    void Update();"
Add-Content -Path "src/$param1.h" -Value "};"