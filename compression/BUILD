package(
    default_visibility = ['//visibility:public']
)


cc_library(
    name  = 'compression_lib',
    srcs = ['compression.cc', 'helpers.cc'],
    hdrs = ['compression.h', 'helpers.h', 'common.h'],
)

cc_binary(
    name  = 'main',
    srcs = ['main.cc'],
    deps = [':compression_lib'],
)

cc_test(
    name = 'compression_test',
    size = 'small',
    copts = ["-Iexternal/gtest/include"],
    srcs = ['compression_test.cc'],
    deps = [
        '@gtest//:main',
        ':compression_lib',
         ],
)