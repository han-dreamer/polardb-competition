#!/bin/bash
# Codex Setup Script

# 1. Create .codex directory
mkdir -p ~/.codex

# 2. Create config.toml
cat > ~/.codex/config.toml << 'EOF'
model_provider = "packycode"
model = "gpt-5" #可更改为model = "gpt-5-codex"
model_reasoning_effort = "high"
disable_response_storage = true

[model_providers.packycode]
name = "packycode"
base_url = "https://codex-api.packycode.com/v1"
wire_api = "responses"
requires_openai_auth = true
EOF

# 3. Create auth.json
cat > ~/.codex/auth.json << 'EOF'
{
  "OPENAI_API_KEY": "sk-eKtKQXdaE2Xk3pKadlvyZ31vC6LKeLKo"
}
EOF

# 4. Start Codex
echo "Setup complete! You can now run 'codex' to start."
codex
