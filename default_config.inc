sgp_driver_dir ?= ..
sensirion_common_dir ?= ${sgp_driver_dir}/embedded-common
sgp_common_dir ?= ${sgp_driver_dir}/sgp-common
sgp30_dir ?= ${sgp_driver_dir}/sgp30
CONFIG_I2C_TYPE ?= hw_i2c

sw_i2c_impl_src ?= ${sensirion_common_dir}/sw_i2c/sensirion_sw_i2c_implementation.c
hw_i2c_impl_src ?= ${sensirion_common_dir}/hw_i2c/sensirion_hw_i2c_implementation.c

CFLAGS ?= -Os -fstrict-aliasing -fPIC
CFLAGS += -I${sensirion_common_dir} -I${sgp_common_dir} -I${sgp30_dir} \
          -I${sensirion_common_dir}/${CONFIG_I2C_TYPE}

sensirion_common_sources = ${sensirion_common_dir}/sensirion_arch_config.h \
                           ${sensirion_common_dir}/sensirion_i2c.h \
                           ${sensirion_common_dir}/sensirion_common.h \
                           ${sensirion_common_dir}/sensirion_common.c

sgp_common_sources = ${sgp_common_dir}/sgp_git_version.h \
                     ${sgp_common_dir}/sgp_git_version.c \

sgp30_sources = ${sensirion_common_sources} \
                ${sgp_common_sources} \
                ${sgp30_dir}/sgp30.h ${sgp30_dir}/sgp30.c

hw_i2c_sources = ${hw_i2c_impl_src}
sw_i2c_sources = ${sensirion_common_dir}/sw_i2c/sensirion_sw_i2c_gpio.h \
                 ${sensirion_common_dir}/sw_i2c/sensirion_sw_i2c.c \
                 ${sw_i2c_impl_src}
