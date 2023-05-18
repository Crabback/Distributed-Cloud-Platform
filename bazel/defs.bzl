LINKOPTS = select({
    "@platforms//os:macos": ["-undefined error -framework CoreFoundation"],  # weird absl bug
    "//conditions:default": [],
})
