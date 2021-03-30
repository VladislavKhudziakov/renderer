sudo apt-get install -y libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev vulkan-tools libvulkan-dev vulkan-validationlayers-dev spirv-tools curl
mkdir ./downloads
curl https://storage.googleapis.com/shaderc/artifacts/prod/graphics_shader_compiler/shaderc/linux/continuous_clang_release/356/20210315-120038/install.tgz -o ./downloads/glslang.tgz
mkdir ./downloads/glslang
tar -C ./downloads/glslang -xvf ./downloads/glslang.tgz
mkdir -p ./tools/glsl
cp downloads/glslang/install/bin/* ./tools/glsl/