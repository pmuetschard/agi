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
# Java client dependencies.

load("@gapid//tools/build/rules:repository.bzl", "github_repository", "maybe_repository", "maven_jar")
load("@gapid//tools/build/third_party:jface.bzl", "jface")
load("@gapid//tools/build/third_party:swt.bzl", "swt")

# Defines the repositories for GAPID's Java client's dependencies.
#  no_maven - if true, none of the maven managed dependencies are initialized.
#  no_swt - if true, the SWT repository is not initialized.
#  no_jface - if true, the JFace repository is not initialized.
#  locals - can be used to provide local path overrides for repos:
#     {"foo": "/path/to/foo"} would cause @foo to be a local repo based on /path/to/foo.
def gapic_dependencies(no_maven = False, no_swt = False, no_jface = False, locals = {}):

    maybe_repository(
        github_repository,
        name = "com_github_grpc_java",
        locals = locals,
        organization = "grpc",
        project = "grpc-java",
        commit = "a5323492654dfeaf7de2fca06bc72be60b31c1e0",  # 1.36.0
        sha256 = "ee72d4d98a19cee59651601a81cf8b65e7fd9f9ce1736762c4d41e64b8ff2a39"
    )

    if not no_maven:
        # gRPC and it's dependencies.
        ########################################################################
        maybe_repository(
            maven_jar,
            name = "io_grpc_api",
            locals = locals,
            artifact = "io.grpc:grpc-api:1.36.0",
            sha256 = "3226c41a2d08a5158632001760dacb951165548d4a4248062aafa5bf2c00b10f",
            sha256_src = "fd7d20ca189d555ec3c460a9eed8ba67369bd6497c51eb20bc4c825dd701f19d",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_context",
            locals = locals,
            artifact = "io.grpc:grpc-context:1.36.0",
            sha256 = "2cc9440617bb8e644435ee5d2ea3fb149eca8f7689e33e2e173ba84b573549e4",
            sha256_src = "366a2f0ed6cc8d66e56eb71f3b6122fda6f74373fe04da9288e727366a82100c",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_core",
            locals = locals,
            artifact = "io.grpc:grpc-core:1.36.0",
            sha256 = "dcdf193caa7f772eb794cfce4e005c477dcb8870009e0f8abfc41f8de38eaa24",
            sha256_src = "54400629b279e80595afece40c7315769551a289747179589a242e572251febf",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_okhttp",
            locals = locals,
            artifact = "io.grpc:grpc-okhttp:1.36.0",
            sha256 = "7bcb0311c339e0767f2a383845c19f3a9a0364437554d3e7d5fc0be78af4bd31",
            sha256_src = "3cc925162813f8a1793b9a3291821ca15c9302ab63b562c87a7f70a55d4a4e4b",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_protobuf",
            locals = locals,
            artifact = "io.grpc:grpc-protobuf:1.36.0",
            sha256 = "0aec2713ff54ffe40b3c6411c26b233a8e0ab93fe8b7a494575047282a3801be",
            sha256_src = "d8d8e72071934a9417f471d16919d0d2cfc78c1f1593bd0c6224bd719900d312",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_protobuf_lite",
            locals = locals,
            artifact = "io.grpc:grpc-protobuf-lite:1.36.0",
            sha256 = "c23ee011bb630e9aec3994a8731a2920327cd1b3657584edc8b83d5aa47e2a42",
            sha256_src = "42b4e3a6bc36bdecaaa70f4fe803970ddcc3905249b0b9c14ee2258ceee41867",
        )

        maybe_repository(
            maven_jar,
            name = "io_grpc_stub",
            locals = locals,
            artifact = "io.grpc:grpc-stub:1.36.0",
            sha256 = "c715c938bf29b210348d25116d906c8d225acb0a6d2b321b0c1f18e4602ce036",
            sha256_src = "a43dbf45c85f91c8b3873f70563d55805711c9285aa1086a1728614ccd443943",
        )

        # OKHttp used by gRPC.
        maybe_repository(
            maven_jar,
            name = "com_squareup_okhttp",
            locals = locals,
            artifact = "com.squareup.okhttp:okhttp:2.7.4",
            sha256 = "c88be9af1509d5aeec9394a818c0fa08e26fad9d64ba134e6f977e0bb20cb114",
            sha256_src = "57c3b223fb40568eabb97e2be989625746af99120a8112bbcfa49d7d9ab3c746",
        )

        maybe_repository(
            maven_jar,
            name = "com_squareup_okio",
            locals = locals,
            artifact = "com.squareup.okio:okio:1.17.5",
            sha256 = "19a7ff48d86d3cf4497f7f250fbf295f430c13a528dd5b7b203f821802b886ad",
            sha256_src = "537b41075d390d888aec040d0798211b1702d34f558efc09364b5f7d388ec496",
        )

        # Opencensus used by gRPC.
        maybe_repository(
            maven_jar,
            name = "io_opencensus_api",
            locals = locals,
            artifact = "io.opencensus:opencensus-api:0.28.0",
            sha256 = "0c1723f3f6d3061323845ce8b88b35fdda500812e0a75b8eb5fcc4ad8c871a95",
            sha256_src = "0c6aedc3a87be3b8110eeeb8d7df84d68c3b79831247ddf422d14a2c5faa5fd1",
        )

        maybe_repository(
            maven_jar,
            name = "io_opencensus_contrib_grpc_metrics",
            locals = locals,
            artifact = "io.opencensus:opencensus-contrib-grpc-metrics:0.28.0",
            sha256 = "3d0cac023d5ee251d89f14b10666455f747cb897fd8ba8e4a64ccdfc619f701c",
            sha256_src = "84118e73878eab59b0f09dc476def7363c0ccd3e07709637a85beac913cd896e",
        )

        # Perfmark used by gRPC.
        maybe_repository(
            maven_jar,
            name = "io_perfmark_api",
            locals = locals,
            artifact = "io.perfmark:perfmark-api:0.23.0",
            sha256 = "c705b5c10c18ff3032b9e81742bc2f6b0e5607f6a6dfc0c8ad0cff75d4913042",
            sha256_src = "8b75ae9cac9c14c8b697501adf74584130a307f2851d135e0ada4667cdf3b7b5",
        )

        maybe_repository(
            maven_jar,
            name = "javax_annotation_api",
            locals = locals,
            artifact = "javax.annotation:javax.annotation-api:1.2",
            sha256 = "5909b396ca3a2be10d0eea32c74ef78d816e1b4ead21de1d78de1f890d033e04",
            sha256_src = "8bd08333ac2c195e224cc4063a72f4aab3c980cf5e9fb694130fad41689689d0",
        )

        # LWJGL.
        ############################################################################
        maybe_repository(
            maven_jar,
            name = "org_lwjgl_core",
            locals = locals,
            artifact = "org.lwjgl:lwjgl:3.2.3",
            sha256 = "f9928c3b4b540643a1bbd59286d3c7175e470849261a0c29a81389f52265ad8b",
            sha256_src = "97b9c693337f76a596b86b07db26a0a8022e3a4e0a0360edb9bb87bc9b172cda",
            sha256_linux = "002810129fc6ac4cdfcdf190e18a643a5021b6300f489c1026bbc5d00140ca2e",
            sha256_windows = "bdf519b9aa90f799954113a15dfa84b273ee4781876b3ecdebf192ce4f88a26c",
            sha256_macos = "5c520c465a84034b8bc23e1d7ecd621bb99c437cd254ea46b53197448d1b8128",
        )

        maybe_repository(
            maven_jar,
            name = "org_lwjgl_opengl",
            locals = locals,
            artifact = "org.lwjgl:lwjgl-opengl:3.2.3",
            sha256 = "10bcc37506e01d1477d65f1fcf0aa672c95eb785265b28b7f321c8381093eda2",
            sha256_src = "6082a81f350dfc0e390a9ceb4347fa2a28cd07dfd54dc757fb05fa6f3350314e",
            sha256_linux = "466e8bae1818c4c584771ee093c8a735e26f56fb25a81dde5675160aaa2fa045",
            sha256_windows = "c08e3de31632163ac5f746fa945f1924142e08520bd9c81b7dd1b5dbd1b0b8bb",
            sha256_macos = "e4b4d0cd9138d52271c1d5c18e43c9ac5d36d1a727c47e5ee4031cb45ce730ca",
        )

        # Other dependencies.
        ############################################################################
        maybe_repository(
            maven_jar,
            name = "com_google_guava",
            locals = locals,
            artifact = "com.google.guava:guava:30.1-jre",
            sha256 = "e6dd072f9d3fe02a4600688380bd422bdac184caf6fe2418cfdd0934f09432aa",
            sha256_src = "b17d4974b591e7e45d982d04ce400c424fa95288cbddce17394b65f65bfdec0f",
        )

        maybe_repository(
            maven_jar,
            name = "com_google_guava-failureaccess",
            locals = locals,
            artifact = "com.google.guava:failureaccess:1.0.1",
            sha256 = "a171ee4c734dd2da837e4b16be9df4661afab72a41adaf31eb84dfdaf936ca26",
            sha256_src = "092346eebbb1657b51aa7485a246bf602bb464cc0b0e2e1c7e7201fadce1e98f",
        )

    if not no_swt:
        maybe_repository(
            swt,
            name = "swt",
            locals = locals,
        )

    if not no_jface:
        maybe_repository(
            jface,
            name = "jface",
            locals = locals,
        )

DEFAULT_MAPPINGS = {
    # gRPC
    "io_grpc_api": "@io_grpc_api//:jar",
    "io_grpc_context": "@io_grpc_context//:jar",
    "io_grpc_core": "@io_grpc_core//:jar",
    "io_grpc_okhttp": "@io_grpc_okhttp//:jar",
    "io_grpc_protobuf": "@io_grpc_protobuf//:jar",
    "io_grpc_protobuf_lite": "@io_grpc_protobuf_lite//:jar",
    "io_grpc_stub": "@io_grpc_stub//:jar",
    "com_squareup_okhttp": "@com_squareup_okhttp//:jar",
    "com_squareup_okio": "@com_squareup_okio//:jar",
    "io_opencensus_api": "@io_opencensus_api//:jar",
    "io_opencensus_contrib_grpc_metrics": "@io_opencensus_contrib_grpc_metrics//:jar",
    "io_perfmark_api": "@io_perfmark_api//:jar",
    "javax_annotation_api": "@javax_annotation_api//:jar",
    # LWJGL
    "org_lwjgl_core": "@org_lwjgl_core//:jar",
    "org_lwjgl_core_natives_linux": "@org_lwjgl_core//:jar-natives-linux",
    "org_lwjgl_core_natives_windows": "@org_lwjgl_core//:jar-natives-windows",
    "org_lwjgl_core_natives_macos": "@org_lwjgl_core//:jar-natives-macos",
    "org_lwjgl_opengl": "@org_lwjgl_opengl//:jar",
    "org_lwjgl_opengl_natives_linux": "@org_lwjgl_opengl//:jar-natives-linux",
    "org_lwjgl_opengl_natives_windows": "@org_lwjgl_opengl//:jar-natives-windows",
    "org_lwjgl_opengl_natives_macos": "@org_lwjgl_opengl//:jar-natives-macos",
    # Others
    "com_google_guava": "@com_google_guava//:jar",
    "com_google_guava-failureaccess": "@com_google_guava-failureaccess//:jar",
    "jface": "@jface",
    "swt": "@swt",
}

def gapic_third_party(mappings = DEFAULT_MAPPINGS):
    _gapic_third_party(
        name = "gapic_third_party",
        mappings = mappings,
    )

def _gapic_third_party_impl(ctx):
    ctx.template(
        ctx.path("BUILD.bazel"),
        Label("@gapid//tools/build/third_party:gapic_third_party.BUILD"),
        substitutions = {
            k.join(["{{", "}}"]): ctx.attr.mappings[k] for k in ctx.attr.mappings
        },
        executable = False,
    )

_gapic_third_party = repository_rule(
    implementation = _gapic_third_party_impl,
    attrs = {
        "mappings": attr.string_dict(
            allow_empty = False,
            mandatory = True,
        ),
    },
)
