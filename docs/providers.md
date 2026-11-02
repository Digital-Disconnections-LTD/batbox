# Model providers

## Amazon Bedrock

Batbox calls Anthropic models on Bedrock through `InvokeModelWithResponseStream`
(and `InvokeModel` when a request is not streamed). The request body is the
Messages API body without `model`, with `anthropic_version: "bedrock-2023-05-31"`.
Bedrock's binary event stream is decoded back into Messages API stream events, so
Flights, gadgets and resume work as they do with any other endpoint.

### Endpoint record

```json
{
  "profile": "bedrock::us.anthropic.claude-opus-5-5-v1:0",
  "endpoints": {
    "bedrock": {
      "label": "Amazon Bedrock",
      "provider": "bedrock",
      "region": "us-west-2",
      "models": ["us.anthropic.claude-opus-5-5-v1:0"],
      "context_window": 1000000,
      "max_output_tokens": 128000
    }
  }
}
```

- `region` picks `https://bedrock-runtime.{region}.amazonaws.com`. Without it,
  Batbox uses `AWS_REGION`, then `AWS_DEFAULT_REGION`, then the profile's
  `region` in `~/.aws/config`, then `us-east-1`.
- `base_url` is optional. Set it to reach Bedrock Runtime through a VPC interface
  endpoint, for example
  `https://vpce-0123-abcd.bedrock-runtime.us-west-2.vpce.amazonaws.com`.
  `AWS_ENDPOINT_URL_BEDROCK_RUNTIME` and `AWS_ENDPOINT_URL` work too.
- Model ids are Bedrock ids: an inference profile such as
  `us.anthropic.claude-opus-5-5-v1:0` or `global.anthropic.claude-sonnet-5-5-v1:0`,
  a base model id, or an ARN. Newer Claude models are only offered through
  inference profiles.
- **Discover models** lists the region's Anthropic inference profiles and the
  Anthropic base models that support on-demand use. It calls
  `https://bedrock.{region}.amazonaws.com` (`AWS_ENDPOINT_URL_BEDROCK` overrides it).

### Credentials

Batbox looks for credentials in this order and uses the first it finds:

1. The endpoint's API key (`auth_token`, `api_key_env` or `api_key_helper`), sent
   as a Bedrock API key.
2. `AWS_BEARER_TOKEN_BEDROCK`, a Bedrock API key. Requests carry it as a bearer
   token and are not signed.
3. `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`, with `AWS_SESSION_TOKEN` for
   temporary credentials. Requests are signed with Signature Version 4.
4. The profile named by `AWS_PROFILE` (or `default`) in `~/.aws/credentials` and
   `~/.aws/config` (`AWS_SHARED_CREDENTIALS_FILE` and `AWS_CONFIG_FILE` move them):
   - `role_arn` with `source_profile` (or `credential_source = Environment`):
     Batbox calls STS `AssumeRole` with the source credentials.
     `role_session_name`, `duration_seconds` and `external_id` are honoured.
     `AWS_ENDPOINT_URL_STS` overrides the STS endpoint.
   - `aws_access_key_id` and `aws_secret_access_key`, with optional
     `aws_session_token`.
   - `credential_process`: the command's JSON output (`Version: 1`) is reused
     until a few minutes before its `Expiration`.

Resolved credentials stay in memory until five minutes before they expire. A
response saying the token expired clears them, and the request is sent once more.

IAM Identity Center (SSO) profiles go through `credential_process`. AWS CLI v2
prints SSO credentials in that format:

```ini
[profile bedrock]
region = us-west-2
credential_process = aws configure export-credentials --profile my-sso-profile --format process
```

Run `aws sso login --profile my-sso-profile` when the SSO session lapses.
Batbox does not read the EC2 instance metadata service, ECS container
credentials, or web identity tokens; on those hosts, use `credential_process`
with the AWS CLI command above.

### IAM

The calling identity needs:

- `bedrock:InvokeModelWithResponseStream` and `bedrock:InvokeModel` on the model
  or inference profile. For a cross-region inference profile, grant them on the
  profile ARN and on the underlying foundation models in each destination region.
- `bedrock:ListFoundationModels` and `bedrock:ListInferenceProfiles` for
  **Discover models**.
- `sts:AssumeRole` on the role, when a profile uses `role_arn`.

Anthropic models can require a one-time use case submission in the Bedrock
console before the first call. Until then, requests fail with
`AccessDeniedException`.

### Errors

Bedrock errors appear as Messages API errors, so retry and messages behave as
they do elsewhere. `ThrottlingException` and `ServiceQuotaExceededException` are
rate limit errors (429). `ModelNotReadyException` and
`ServiceUnavailableException` are overloaded errors and are retried.
`ValidationException` is an invalid request (400). `AccessDeniedException` is a
permission error (403). Expired or invalid credentials are authentication errors (401).

## Google Vertex AI

Batbox calls Claude on Vertex AI with `streamRawPredict` (and `rawPredict` for
requests that are not streamed). The body is the Messages API body without
`model`, with `anthropic_version: "vertex-2023-10-16"`. The response stream is
ordinary server-sent events.

### Endpoint record

```json
{
  "profile": "vertex::claude-opus-5-5",
  "endpoints": {
    "vertex": {
      "label": "Vertex AI",
      "provider": "vertex",
      "project": "my-gcp-project",
      "region": "global",
      "models": ["claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5"],
      "context_window": 1000000,
      "max_output_tokens": 128000
    }
  }
}
```

- `project` is required. Without it, Batbox uses `GOOGLE_VERTEX_PROJECT`,
  `GOOGLE_CLOUD_PROJECT`, `GCP_PROJECT`, `GCLOUD_PROJECT`, then the `project_id`
  in the credentials file.
- `region` is a Vertex location such as `us-east5` or `europe-west1`, or `global`.
  `global` uses `https://aiplatform.googleapis.com`; any other region uses
  `https://{region}-aiplatform.googleapis.com`. Without it, Batbox uses
  `GOOGLE_VERTEX_LOCATION`, `GOOGLE_CLOUD_LOCATION`, `VERTEX_LOCATION`, then `global`.
- `base_url` is optional and replaces the host, for example a Private Service
  Connect endpoint.
- Vertex has no inexpensive way to list Anthropic models, so **Discover models**
  returns the endpoint's `models`, or Batbox's built-in list of Claude model ids
  when `models` is empty. Model ids are the Vertex names, such as
  `claude-sonnet-5-5`; dated names such as `claude-3-5-sonnet-v2@20241022` also work.

### Credentials

Batbox mints an OAuth access token for the `cloud-platform` scope, in this order:

1. The endpoint's API key (`auth_token`, `api_key_env` or `api_key_helper`), used
   as a ready access token. `"api_key_helper": "gcloud auth print-access-token"`
   is one way to supply it.
2. `GOOGLE_APPLICATION_CREDENTIALS`, pointing at a service-account key file
   (Batbox signs a JWT and exchanges it at the file's `token_uri`) or at an
   `authorized_user` file.
3. The file `gcloud auth application-default login` writes
   (`~/.config/gcloud/application_default_credentials.json`, or under
   `CLOUDSDK_CONFIG`).
4. `gcloud auth print-access-token`.
5. The metadata server on Compute Engine, GKE and Cloud Run
   (`GCE_METADATA_HOST` overrides its address).

Tokens are reused until a minute before they expire. A token from `gcloud` is
reused for fifteen minutes. A 401 response discards the token, and the request
is sent once more.

### IAM

- Enable the Vertex AI API in the project and enable each Claude model you use
  from its Model Garden page.
- Give the calling identity the Vertex AI User role (`roles/aiplatform.user`),
  or another role with `aiplatform.endpoints.predict`.
- Quotas are per model and region. Requests over quota return
  `RESOURCE_EXHAUSTED`, which Batbox reports as a rate limit error (429).

### Errors

Google RPC errors are mapped by status: `RESOURCE_EXHAUSTED` is a rate limit
error, `UNAVAILABLE` an overloaded error, `UNAUTHENTICATED` an authentication
error, `PERMISSION_DENIED` a permission error, `NOT_FOUND` not found, and
`INVALID_ARGUMENT` an invalid request. Errors the model returns in Messages API
form pass through unchanged.
