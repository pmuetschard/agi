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
# go dependencies.

load("@gapid//tools/build/rules:repository.bzl", "github_http_args")
load("@bazel_gazelle//:deps.bzl", "go_repository")

# Defines the repositories for GAPID's go dependencies.
# After calling gapid_dependencies(), load @bazel_gazelle's
# go_repository and call this macro.
def gapid_go_dependencies():
    _maybe(_github_go_repository,
        name = "com_github_google_go_github",
        organization = "google",
        project = "go-github",
        commit = "6c3a8a15c7c49134d4d2f4b42a8c2edf530ced14",  # 33.0.0
        importpath = "github.com/google/go-github",
        sha256 = "1e2901d2dab3e7d7c7c12e6e81bfd3afe308fab2ab2c23ce035ff34763bd1bd1",
    )

    # Dependency of com_github_google_go_github.
    _maybe(_github_go_repository,
        name = "com_github_google_go_querystring",
        organization = "google",
        project = "go-querystring",
        commit = "55faf79c297ffe9e26d034acafd0c9f614ad9da9",
        importpath = "github.com/google/go-querystring",
        sha256 = "5990217e122cad015e3caedb1fcdfd658483864e7d2844784915aa7d53e4ef74",
    )

    _maybe(_github_go_repository,
        name = "com_github_pkg_errors",
        organization = "pkg",
        project = "errors",
        commit = "614d223910a179a466c1767a985424175c39b465",  # 0.9.1
        importpath = "github.com/pkg/errors",
        sha256 = "49c7041442cc15211ee85175c06ffa6520c298b1826ed96354c69f16b6cfd13b",
    )

    _maybe(_github_go_repository,
        name = "org_golang_google_grpc",
        organization = "grpc",
        project = "grpc-go",
        commit = "f74f0337644653eba7923908a4d7f79a4f3a267b",  # 1.36.0
        importpath = "google.golang.org/grpc",
        sha256 = "f6bd026b62bde7703731a5c02f956fe44021c172c2aec5e97a999f8891f8e7f7",
    )

    _maybe(_github_go_repository,
        name = "org_golang_x_crypto",
        organization = "golang",
        project = "crypto",
        commit = "5ea612d1eb830b38bc4e914e37f55311eb58adce",
        importpath = "golang.org/x/crypto",
        sha256 = "98577f4d27b16f827918c0d5270228585b4b02f9ee2ff1d12acf38f6af0cf7f5",
    )

    # Dependency of org_golang_x_tools.
    _maybe(_github_go_repository,
        name = "org_golang_x_mod",
        organization = "golang",
        project = "mod",
        commit = "6ce8bb3f08e0e47592fe93e007071d86dcf214bb",  # 0.4.1
        importpath = "golang.org/x/mod",
        sha256 = "7b5008b98e341459375f1de23007b09339d1fe5d6f3c5e4b4ed7d12a002471de",
    )

    _maybe(_github_go_repository,
        name = "org_golang_x_net",
        organization = "golang",
        project = "net",
        commit = "e18ecbb051101a46fc263334b127c89bc7bff7ea",
        importpath = "golang.org/x/net",
        sha256 = "e7ed488428e41afb7f1f69f952d6fd75cccb31f2e2540c06c2d38563bd697de9",
    )

    # Dependency of org_golang_x_net.
    _maybe(_github_go_repository,
        name = "org_golang_x_text",
        organization = "golang",
        project = "text",
        commit = "e3aa4adf54f644ca0cb35f1f1fb19b239c40ef04",
        importpath = "golang.org/x/text",
        sha256 = "abfdd8c49e0895a7e6db346a9f83602ff398d5a34592b21a77403eb6a7c65d12",
    )

    # Dependency of org_golang_x_mod.
    _maybe(_github_go_repository,
        name = "org_golang_x_xerrors",
        organization = "golang",
        project = "xerrors",
        commit = "5ec99f83aff198f5fbd629d6c8d8eb38a04218ca",
        importpath = "golang.org/x/xerrors",
        sha256 = "cd9de801daf63283be91a76d7f91e8a9541798c5c0e8bcfb7ee804b78a493b02",
    )


def _maybe(repo_rule, name, **kwargs):
    if name not in native.existing_rules():
        repo_rule(name = name, **kwargs)

def _github_go_repository(name, organization, project, commit, **kwargs):
    github = github_http_args(
        organization = organization,
        project = project,
        commit = commit,
    )
    go_repository(
        name = name,
        urls = [ github.url ],
        type = github.type,
        strip_prefix = github.strip_prefix,
        **kwargs
    )
