#!/bin/bash
# Portions Copyright (c) Microsoft Corporation
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

#######################################
# Copy build artifacts to the workspace's dist/azure.
# Arguments:
#   * the docker image tar URI
# Globals:
#   WORKSPACE
#######################################
function create_azure_dist() {
  local -r server_image="$1"
  local -r dist_dir="${WORKSPACE}/dist"
  mkdir -p "${dist_dir}"/azure
  chmod 770 "${dist_dir}" "${dist_dir}"/azure
  cp "${WORKSPACE}/${server_image}" "${dist_dir}"/azure
}
