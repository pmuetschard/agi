# Copyright (C) 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Defines macros to be called from a WORKSPACE file to setup the GAPID
# dependencies and toolchains.

load("@gapid//tools/build:cc_toolchain.bzl", "cc_configure")
load("@gapid//tools/build/rules:android.bzl", "android_native_app_glue", "ndk_vk_validation_layer", "ndk_version_check")
load("@gapid//tools/build/rules:repository.bzl", "github_repository", "maybe_repository")
load("@gapid//tools/build/third_party:breakpad.bzl", "breakpad")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Defines the repositories for GAPID's dependencies, excluding the
# go dependencies, which require @io_bazel_rules_go to be setup.
#  android - if false, the Android NDK/SDK are not initialized.
#  mingw - if false, our cc toolchain, which uses MinGW on Windows is not initialized.
#  locals - can be used to provide local path overrides for repos:
#     {"foo": "/path/to/foo"} would cause @foo to be a local repo based on /path/to/foo.
def gapid_dependencies(android = True, mingw = True, locals = {}):
    #####################################################
    # Get repositories with workspace rules we need first

    maybe_repository(
        github_repository,
        name = "io_bazel_rules_go",
        locals = locals,
        organization = "bazelbuild",
        project = "rules_go",
        commit = "ddc5747aa3a52e7703f48281d82ea06985d95628",  # 0.26.0
        sha256 = "15f79296dc53ce569308288ee50fd75dac0bb35242955e68da921e647b106909",
    )

    maybe_repository(
        github_repository,
        name = "bazel_gazelle",
        locals = locals,
        organization = "bazelbuild",
        project = "bazel-gazelle",
        commit = "e9091445339de2ba7c01c3561f751b64a7fab4a5",  # 0.23.0
        sha256 = "03e266ed67fd21f6fbede975773a569d397312daae71980d34ff7f7e087b7b14",
    )

    maybe_repository(github_repository,
        name = "net_zlib", # name used by rules_go
        locals = locals,
        organization = "madler",
        project = "zlib",
        commit = "cacf7f1d4e3d44d871b605da3b647f07d718623f",
        build_file = "@gapid//tools/build/third_party:zlib.BUILD",
        sha256 = "1cce3828ec2ba80ff8a4cac0ab5aa03756026517154c4b450e617ede751d41bd",
    )

    maybe_repository(
        github_repository,
        name = "com_google_protobuf",
        locals = locals,
        organization = "google",
        project = "protobuf",
        commit = "4fff47a41811eeaef8add8def480062282292ce5",  # 3.15.5
        sha256 = "d2789ef50c30716749d45ec7ff91be5aac2041ef8d3889f653a139a6d3340e0b",
        repo_mapping = {"@zlib": "@net_zlib"},
    )

    maybe_repository(
        github_repository,
        name = "com_github_grpc_grpc",
        locals = locals,
        organization = "grpc",
        project = "grpc",
        commit = "c3438a0c5d7bc499eb31fd4853ca72c771f758a5",  # 1.36.2
        sha256 = "359db066ea7d8a54d343d95efa0e30192c39fa1a55e00af3df6c16748018d45c",
        repo_mapping = {"@zlib": "@net_zlib"},
    )

    ###########################################
    # Now get all our other non-go dependencies

    maybe_repository(
        github_repository,
        name = "com_google_googletest",
        locals = locals,
        organization = "google",
        project = "googletest",
        commit = "703bd9caab50b139428cea1aaff9974ebee5742e",  # 1.10.0
        sha256 = "2db427be8b258ad401177c411c2a7c2f6bc78548a04f1a23576cc62616d9cd38",
    )

    maybe_repository(
        github_repository,
        name = "astc_encoder",
        locals = locals,
        organization = "ARM-software",
        project = "astc-encoder",
        commit = "2164984243a3fbd2b2429e7562f7fe31914f446b",  # 2.1 (November 2020)
        build_file = "@gapid//tools/build/third_party:astc-encoder.BUILD",
        sha256 = "50d0e8c84acf7e16d0ce2c4f3bf658c5a69d0e07d21b7fd629b73cbb143b477a",
    )

    maybe_repository(
        github_repository,
        name = "etc2comp",
        locals = locals,
        organization = "google",
        project = "etc2comp",
        commit = "9cd0f9cae0f32338943699bb418107db61bb66f2", # 2017/04/24
        build_file = "@gapid//tools/build/third_party:etc2comp.BUILD",
        sha256 = "0ddcf7484c0d55bc5a3cb92edb4812dc932ac9f73b4641ad2843fec82ae8cf90",
    )

    maybe_repository(
        breakpad,
        name = "breakpad",
        locals = locals,
        commit = "a61afe7a3e865f1da7ff7185184fe23977c2adca",
        build_file = "@gapid//tools/build/third_party/breakpad:breakpad.BUILD",
    )

    maybe_repository(
        github_repository,
        name = "cityhash",
        locals = locals,
        organization = "google",
        project = "cityhash",
        commit = "8af9b8c2b889d80c22d6bc26ba0df1afb79a30db",
        build_file = "@gapid//tools/build/third_party:cityhash.BUILD",
        sha256 = "3524f5ed43143974a29fddeeece29c8b6348f05db08dd180452da01a2837ddce",
    )

    # Override the gRPC abseil dependency, so we can patch it.
    maybe_repository(
        github_repository,
        name = "com_google_absl",
        locals = locals,
        organization = "abseil",
        project = "abseil-cpp",
        commit = "0f3bb466b868b523cf1dc9b2aaaed65c77b28862",  # LTS 20200923, Patch 2
        sha256 = "9929f3662141bbb9c6c28accf68dcab34218c5ee2d83e6365d9cb2594b3f3171",
        patches = [
            # Workaround for https://github.com/abseil/abseil-cpp/issues/326.
            "@gapid//tools/build/third_party:abseil_macos_fix.patch",
            # Allows building on Windows with MinGW.
            "@gapid//tools/build/third_party:abseil_windows_fix.patch",
        ],
    )

    maybe_repository(
        github_repository,
        name = "glslang",
        locals = locals,
        organization = "KhronosGroup",
        project = "glslang",
        commit = "740ae9f60b009196662bad811924788cee56133a",  # 10-11.0.0
        sha256 = "c015e7d81c0a248562c25a1e484fb8528eb6a765312cf5de3bdb658b03562b3f",
    )

    maybe_repository(
        github_repository,
        name = "llvm",
        locals = locals,
        organization = "llvm-mirror",
        project = "llvm",
        commit = "e562960fe303c0ffab6f3458fcdb1544b56fd81e",
        build_file = "@gapid//tools/build/third_party:llvm.BUILD",
        sha256 = "3ef3d905849d547b6481b16d8e7b473a84efafbe90131e7bc90a0c6aae4cd8e6",
        # This patch fixes missing standard library includes which leads to compilation
        # issues in recent gcc. This issue is fixed on recent llvm versions (since
        # https://github.com/llvm-mirror/llvm/commit/e0402b5c9813a2458b8dd3f640883110db280395),
        # but updating our llvm version leads to other errors.
        patches = [
            "@gapid//tools/build/third_party:llvm_fix.patch",
        ],
    )

    maybe_repository(
        new_git_repository,
        name = "stb",
        locals = locals,
        remote = "https://github.com/nothings/stb",
        commit = "f54acd4e13430c5122cab4ca657705c84aa61b08",
        build_file = "@gapid//tools/build/third_party:stb.BUILD",
        shallow_since = "1580905940 -0800",
    )

    maybe_repository(
        new_git_repository,
        name = "lss",
        locals = locals,
        remote = "https://chromium.googlesource.com/linux-syscall-support",
        commit = "fd00dbbd0c06a309c657d89e9430143b179ff6db",
        build_file = "@gapid//tools/build/third_party:lss.BUILD",
        shallow_since = "1583885669 +0000",
    )

    maybe_repository(
        git_repository,
        name = "perfetto",
        locals = locals,
        remote = "https://android.googlesource.com/platform/external/perfetto",
        commit = "d1a7b031bbded67e0d67957974e85a83e0b815c0",
        shallow_since = "1619185617 +0100",
    )

    maybe_repository(
        http_archive,
        name = "sqlite",
        locals = locals,
        url = "https://storage.googleapis.com/perfetto/sqlite-amalgamation-3250300.zip",
        sha256 = "2ad5379f3b665b60599492cc8a13ac480ea6d819f91b1ef32ed0e1ad152fafef",
        strip_prefix = "sqlite-amalgamation-3250300",
        build_file = "@perfetto//bazel:sqlite.BUILD",
    )

    maybe_repository(
        http_archive,
        name = "sqlite_src",
        locals = locals,
        url = "https://storage.googleapis.com/perfetto/sqlite-src-3250300.zip",
        sha256 = "c7922bc840a799481050ee9a76e679462da131adba1814687f05aa5c93766421",
        strip_prefix = "sqlite-src-3250300",
        build_file = "@perfetto//bazel:sqlite.BUILD",
    )

    maybe_repository(
        native.new_local_repository,
        name = "perfetto_cfg",
        locals = locals,
        path = "tools/build/third_party/perfetto",
        build_file = "@gapid//tools/build/third_party/perfetto:BUILD.bazel",
    )

    maybe_repository(
        github_repository,
        name = "spirv_headers",
        locals = locals,
        organization = "KhronosGroup",
        project = "SPIRV-Headers",
        commit = "f8bf11a0253a32375c32cad92c841237b96696c0",
        sha256 = "2ca7c37db06ab526c8c5c31767a0bbdbd30de74909dc1a4900302d7a8f537de7",
    )

    maybe_repository(
        github_repository,
        name = "spirv_cross",
        locals = locals,
        organization = "KhronosGroup",
        project = "SPIRV-Cross",
        commit = "871c85d7f0edc6b613e3959bc51d13bfbc2fe2df",
        build_file = "@gapid//tools/build/third_party:spirv-cross.BUILD",
        sha256 = "6aba055d6a9a7c33ec2761c4883b21c9d67c7fef2550797cea677a77fd65055a",
    )

    maybe_repository(
        github_repository,
        name = "spirv_tools",
        locals = locals,
        organization = "KhronosGroup",
        project = "SPIRV-Tools",
        commit = "60104cd97446877dad8ed1010a635218937a2f18",
        sha256 = "6050c012fec919087ebc3b083b24f874648fc1593b55ac8e3742df760aec19fc",
    )

    maybe_repository(
        github_repository,
        name = "spirv_reflect",
        locals = locals,
        organization = "KhronosGroup",
        project = "SPIRV-Reflect",
        commit = "974d7c97be3329732da3aa6b770fbe87805148cb",
        sha256 = "1b2852cffd75ac401f54e21c2786f6c0da9c7199111d8fc55cce17e4ff2b66ce",
    )

    maybe_repository(
        http_archive,
        name = "vscode-languageclient",
        locals = locals,
        url = "https://registry.npmjs.org/vscode-languageclient/-/vscode-languageclient-2.6.3.tgz",
        build_file = "@gapid//tools/build/third_party:vscode-languageclient.BUILD",
        sha256 = "42ad6dc73bbf24a067d1e21038d35deab975cb207ac2d63b81c37a977d431d8f",
    )

    maybe_repository(
        http_archive,
        name = "vscode-jsonrpc",
        locals = locals,
        url = "https://registry.npmjs.org/vscode-jsonrpc/-/vscode-jsonrpc-2.4.0.tgz",
        build_file = "@gapid//tools/build/third_party:vscode-jsonrpc.BUILD",
        sha256= "bed9b2facb7d179f14c8a710db8e613be56bd88b2a75443143778813048b5c89",
    )

    maybe_repository(
        http_archive,
        name = "vscode-languageserver-types",
        locals = locals,
        url = "https://registry.npmjs.org/vscode-languageserver-types/-/vscode-languageserver-types-1.0.4.tgz",
        build_file = "@gapid//tools/build/third_party:vscode-languageserver-types.BUILD",
        sha256 = "0cd219ac388c41a70c3ff4f72d25bd54fa351bc0850196c25c6c3361e799ac79",
    )

    maybe_repository(
        github_repository,
        name = "vulkan-headers",
        locals = locals,
        organization = "KhronosGroup",
        project = "Vulkan-Headers",
        commit = "7264358702061d3ed819d62d3d6fd66ab1da33c3",  # 1.2.132
        build_file = "@gapid//tools/build/third_party:vulkan-headers.BUILD",
        sha256 = "d44112f625cb2152fd7c8906a15e4e98abc5946d1ef85c2e17b3cb5c247586d3",
    )

    if android:
        maybe_repository(
            native.android_sdk_repository,
            name = "androidsdk",
            locals = locals,
            api_level = 26, # This is the target API
        )

        maybe_repository(
            native.android_ndk_repository,
            name = "androidndk",
            locals = locals,
            api_level = 23, # This is the minimum API
        )

        maybe_repository(
            android_native_app_glue,
            name = "android_native_app_glue",
            locals = locals,
        )

        maybe_repository(
            ndk_vk_validation_layer,
            name = "ndk_vk_validation_layer",
            locals = locals,
        )

        maybe_repository(
            ndk_version_check,
            name = "ndk_version_check",
            locals = locals,
        )

    if mingw:
        cc_configure()
