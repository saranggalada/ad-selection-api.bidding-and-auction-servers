# Copilot Instructions

See doc here
<https://docs.github.com/en/copilot/customizing-copilot/about-customizing-github-copilot-chat-responses?tool=vscode>

## Project Overview

Provide a brief description of the project, its purpose, and its main components. For example:

-   This project implements bidding and auction servers for the Ad Selection API initiative.
-   It includes workflows for CI/CD, security analysis, and other automation tasks.
-   Bazel is used for build management of services, ensuring fast and reproducible builds.
-   Terraform is used for infrastructure provisioning and deployment, enabling consistent and
    automated environment setups.

## Coding Standards

Define the coding standards and conventions used in the project. For example:

-   Follow the Google Style Guide for code formatting.
-   Use descriptive variable and function names.
-   Write comments for complex logic.
-   For C++ code, adhere to the
    [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html), including:
    -   Use `snake_case` for variable names and `CamelCase` for class names.
    -   Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers.
    -   Avoid non-const global variables.
    -   Use RAII for resource management.
    -   Write unit tests for all new code.
-   For Azure DevOps pipelines stored in `msft_pipelines`:

    -   Use modular templates to avoid duplication.
    -   Ensure pipeline steps are idempotent.
    -   Use variables and parameterized templates for flexibility.
    -   Include error handling and retries for critical steps.
    -   Validate YAML syntax using tools like `yamllint` or Azure DevOps pipeline validation.
    -   Ensure pipelines inherit from the standard 1ES PT template:

        -   For non-production pipelines, use:

            ```yaml
            template: v1/1ES.Unofficial.PipelineTemplate.yml@1esPipelines
            ```

        -   For production pipelines, use:

            ```yaml
            template: v1/1ES.Official.PipelineTemplate.yml@1esPipelines
            ```

## Workflow Context

Explain the purpose of Azure Pipelines in the `msft_pipelines` directory. For example:

-   Pipelines in this directory are used for CI/CD processes, including building, testing, and
    deploying components of the project.
-   Each pipeline is designed to follow best practices, including modular templates, error handling,
    and compliance with 1ES PT standards.
-   Examples include:
    -   `pr-build-validation.yml`: Validates pull requests by building and testing non-production
        environments.
    -   `internal-validation-build.yml`: Performs internal validation builds for production and
        non-production environments.

## Prompting Guidelines

Provide instructions on how to frame prompts for Copilot. For example:

-   Always specify the file path and the purpose of the change.
-   Mention if new files need to be created or existing ones modified.
-   Provide examples of expected output formats.
-   For Azure pipeline-related prompts:
    -   Specify the file path and whether it is a pipeline or a template.
    -   Mention if you want syntax validation or best practices checks.
    -   Provide examples of expected pipeline behavior or structure.

## Example Prompts

-   **Prompt**: "Validate the syntax of `msft_pipelines/build_pipeline.yaml` and ensure it follows
    best practices."
-   **Expected Response**:

    ```yaml
    # filepath: /home/vlevshukov/src/privacysandbox.bidding-auction-servers/msft_pipelines/build_pipeline.yaml
    # ...existing code...
    # Validation passed: YAML syntax is correct.
    # Best practices check:
    # - Modular templates: OK
    # - Error handling: Missing retries in step 'Deploy'
    # - Variables: All variables are parameterized
    # - Template inheritance: Inherits from `1ES.Unofficial.PipelineTemplate.yml`
    # Suggested Fix: Add retries to the 'Deploy' step.
    ```

-   **Prompt**: "Add a new method `CalculateBid` to the `AuctionService` class in
    `services/auction_service/auction_service.h`."
-   **Expected Response**:

    ```cpp
    // filepath: /home/vlevshukov/src/privacysandbox.bidding-auction-servers/services/auction_service/auction_service.h
    class AuctionService {
        // ...existing code...
    #if defined(MICROSOFT_AD_SELECTION_BUILD)
        double microsoft_CalculateBid(const microsoft_AdRequest& microsoft_request);
    #endif
        // ...existing code...
    };
    ```

-   **Prompt**: "Update the Bazel build file `services/auction_service/BUILD` to include a new
    library target `microsoft_auction_utils`."
-   **Expected Response**:

    ```bazel
    # filepath: privacysandbox.bidding-auction-servers/services/auction_service/BUILD
    # ...existing code...
    cc_library(
        name = "microsoft_auction_utils",
        srcs = ["microsoft_auction_utils.cc"],
        hdrs = ["microsoft_auction_utils.h"],
        deps = [
            "//services/common:common_lib",
        ],
    )
    # ...existing code...
    ```

-   **Prompt**: "Add a retry mechanism to the `Deploy` step in
    `msft_pipelines/deploy_pipeline.yaml`."
-   **Expected Response**:

    ```yaml
    # filepath: /home/vlevshukov/src/privacysandbox.bidding-auction-servers/msft_pipelines/deploy_pipeline.yaml
    # ...existing code...
    - task: AzureCLI@2
      displayName: 'Deploy to Azure'
      inputs:
          scriptType: bash
          scriptLocation: inlineScript
          inlineScript: |
              az deployment group create --resource-group $(ResourceGroup) --template-file $(TemplateFile)
      retryCountOnTaskFailure: 3
      condition: succeededOrFailed()
    # ...existing code...
    ```

## Additional Notes

-   Specify any dependencies or tools used in the project.
-   Mention any sensitive information that should not be included in responses.

-   For custom code added to upstream files:

    -   Wrap the code with `#if defined(MICROSOFT_AD_SELECTION_BUILD)` and `#endif` to distinguish
        it from upstream code. For example:

        ```cpp
        #if defined(MICROSOFT_AD_SELECTION_BUILD)
        // Custom Microsoft-specific code
        #endif
        ```

    -   Prefix variables with `microsoft_` to indicate they are specific to the extension.

-   For new files or folders:

    -   If adding a new folder for custom code, prefix the folder name with `microsoft_`.
    -   If adding a new file to an existing upstream folder, prefix the file name with `microsoft_`.

-   For unit tests:

    -   Prefix unit test names with `Microsoft` to clearly indicate they are specific to the
        extension. For example:

        ```cpp
        TEST(MicrosoftCustomFeatureTest, HandlesEdgeCase) {
          // Test implementation
        }
        ```

        or

        ```cpp
        TEST(ExistingTestClass, MicrosoftCustomFeatureTest) {
          // Test implementation
        }
        ```

-   Ensure that all proposed code adheres to the configurations specified in `.vscode/settings.json`
    and `.pre-commit-config.yaml`. For example:
    -   Follow formatting rules, linting configurations, and any other coding standards defined in
        these files.
    -   Validate that the code passes all pre-commit hooks before submission.
