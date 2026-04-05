# CI/CD

While being publicly hosted on GitHub, this repository is also hosted on a private GitLab instance to play around with automated testing. As it's too much hassle to keep configs, scripts and notes for that separate, it's dumped here.


## Test Pipeline

The test pipeline is defined in `.gitlab-ci.yml`. As it references custom Docker images, it can not run out of the box on any GitLab install. The goals are:

- Assert correct code formatting.
- Compile with GCCs static analyzer to find potential issues.
- Build and run on Ubuntu 24.04 LTS.
- Build all supported [platforms/architectures](../misc/architectures.md).
- Run various tests on [qemu](run_on_qemu.md).

See [getting_started](getting_started.md) on how tests/scripts can run automatically after boot on VIMIX.


## Local Setup

- A Linux machine running GitLab CE.
- Named `pandora`, set in `/etc/hosts` on all clients manually.
- Running on https with self signed certificates.
	- Using http works until custom Docker images have to be pushed to GitLab.
	- All clients must manually add / trust the certificate.
		- Referenced in some scripts/configs but not included.
- Uses custom Docker images as runners:
	- All packages are already installed, speeds up the pipeline significantly.
	- Images are build for `AMD64` and `ARM64` to allow runners on both architectures.
	- Docker files: `tools/docker/ubuntu????` named after the version of the Ubuntu base image.
	- Created and uploaded with `tools/docker/build.sh`.

**Note:** Scripts assume:
- GitLab named `pandora` is present
- `~/.config/gitlab_ssl` contains the self generated certificates (see below on how to generate)


### Updating Docker Images

- Update the Docker files in `tools/docker/ubuntu????`.
- Update `tools/docker/build.sh` if needed (e.g. change target architectures, set certificate to use, GitLab URL).
- Run `tools/docker/build.sh`.


### Add GitLab Runner

#### 1. Install docker and runner

**Arch Linux:**
```bash
sudo pacman -S docker gitlab-runner
```

**Ubuntu:**
```bash
sudo apt update
sudo apt install docker gitlab-runner
```

#### 2. Start + enable service

Optional:
```bash
sudo usermod -aG docker gitlab-runner
sudo systemctl restart gitlab-runner
```

Enable services:
```bash
sudo systemctl enable --now gitlab-runner
sudo systemctl enable --now docker
```

#### 3. Add Server to /etc/hosts

```
192.168.178.X  pandora
```

#### 4. Trust self generated SSL certificates

**Must also be done on the server.**

Assumes these files were generated:
- `ca.crt`
- `pandora.crt`

**System trust (Arch Linux):**
```bash
sudo cp ca.crt /etc/ca-certificates/trust-source/anchors/
sudo trust extract-compat
```

**System trust (Ubuntu):**
```bash
sudo cp ca.crt /usr/local/share/ca-certificates/pandora-ca.crt
sudo update-ca-certificates
```

**Docker trust (both distros):**
```bash
sudo mkdir -p /etc/docker/certs.d/pandora:5050
sudo cp pandora.crt /etc/docker/certs.d/pandora:5050/ca.crt
sudo systemctl restart docker
```

#### 5. Docker login

Use the credintials of your GitLab account.
```bash
docker login pandora:5050
sudo -u gitlab-runner docker login pandora:5050
```

#### 6. Verify setup

```bash
curl https://pandora
curl https://pandora:5050/v2/
docker login pandora:5050
```

No TLS errors are expected, the registry returns `UNAUTHORIZED`.

#### 7. Register the runner

Open GitLab, Settings -> CI/CD -> Runners. Add a runner, select "run untagged jobs", copy the token.
```bash
sudo gitlab-runner register
```

Fill out:
```
URL: https://pandora
Token: <from GitLab>
Executor: docker
```


```bash
sudo gitlab-runner verify
```


#### 8. (Optional) Runner config tuning

Open the runner config in `/etc/gitlab-runner/config.toml`, enable caching of the docker images, optionally set memory and CPU count:
```toml
[runners.docker]
  pull_policy = "if-not-present"
  
  memory = "8g"
  memory_swap = "8g"
  cpus = "4"
```

Restart:
```bash
sudo systemctl restart gitlab-runner
```


### Create Certificates

How the certificates were originally generated on the GitLab server. This is needed for https.

#### 1. Create CA

```bash
sudo mkdir -p /etc/gitlab/ssl
cd /etc/gitlab/ssl

# Generate CA key
openssl genrsa -aes256 -out ca.key 4096

# Generate CA certificate
openssl req -new -x509 -days 3650 -key ca.key -sha256 -out ca.crt
```


#### 2. Create server certificate

**Important:** set `Common Name (CN)` to `pandora`

```bash
# Generate server key
openssl genrsa -out pandora.key 2048

# Create CSR (certificate request)
openssl req -new -key pandora.key -out pandora.csr
```


#### 3. Sign the server certificate

Save as `v3.ext`:

```ini
[ v3_req ]
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = pandora
```


```bash
openssl x509 -req \
  -in pandora.csr \
  -CA ca.crt \
  -CAkey ca.key \
  -CAcreateserial \
  -out pandora.crt \
  -days 3650 \
  -extfile v3.ext \
  -extensions v3_req
```


#### 4. Verify certificate

```bash
# grep must find "DNS:pandora"
openssl x509 -in pandora.crt -text | grep -A2 "Subject Alternative Name"

# expected: "pandora.crt: OK"
openssl verify -CAfile ca.crt pandora.crt
```


#### Resulting files

| File          | Purpose                              |
| ------------- | ------------------------------------ |
| `ca.key`      | CA private key                       |
| `ca.crt`      | CA cert (install on all runners)     |
| `pandora.key` | server private key                   |
| `pandora.csr` | temp file                            |
| `pandora.crt` | server cert (install on all runners) |


---
**Up:** [getting started with the development](getting_started.md)

[automated_tests](automated_tests.md) | [build_instructions](development/build_instructions.md) | [cicd](cicd.md) | [debugging](development/debugging.md) | [overview_directories](development/overview_directories.md) | [run_on_qemu](run_on_qemu.md) | [run_on_spike](run_on_spike.md) | [run_on_visionfive2](run_on_visionfive2.md) 
