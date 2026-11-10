idf_component_register(SRC_DIRS          "."
                                          "src/usecases"
                                          "src/frameworksanddrivers"
                       PRIV_INCLUDE_DIRS  "." "${ESP_MATTER_PATH}/examples/common/utils")

# コルーチン(C++20)を使うため、プロジェクト共通の -std=gnu++17 より後ろで上書きする
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H" "$<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>")
