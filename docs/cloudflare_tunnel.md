# This is instruction for installing cloudflare tunnel for project

# Install first
# Windows: Pobierz z https://github.com/cloudflare/cloudflared/releases
# Linux: wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
# sudo dpkg -i cloudflared-linux-amd64.deb

# Login to cloudflare (for headless just copy the link in browser, link is provided by this command)
cloudflared tunnel login

# Create tunel (it will give you tunnel ID, optionally use 'cloudflared tunnel list')
cloudflared tunnel create tunel-name

# Replace tunnel-id, then paste following into 'sudo nano /etc/cloudflared/config.yml'
tunnel: <tunnel-id>
credentials-file: /root/.cloudflared/<tunnel-id>.json

ingress:
  - hostname: comm.sqrll.net
    service: https://localhost:443
    originRequest:
      noTLSVerify: true
      httpHostHeader: comm.sqrll.net
  - service: http_status:404
  
# the file above makes another ssl, the one from app could be turned off but I do not see why, so we have two

# Next step - Ensure your record does not exist in cloudflare otherwise you will see error
cloudflared tunnel route dns sqrll-comm comm.sqrll.net

# Run tunnel
cloudflared tunnel run sqrll-comm

# Installing service
cloudflared service install
systemctl enable --now cloudflared

# Note: There is a chance that you will get configs in your user dir, in such case just move it
sudo mv ~/.cloudflared/* /etc/cloudflared/

# Use this command to restart and check status
sudo systemctl restart cloudflared
sudo systemctl status cloudflared

# status should show 'Active: active (running)'