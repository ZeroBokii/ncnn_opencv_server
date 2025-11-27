# /etc/systemd/system/ncnn-server.service
[Unit]
Description=NCNN OpenCV Inference Server
After=network.target

[Service]
Type=simple
User=torch
WorkingDirectory=/home/torch/development/ncnn_opencv_server/workspace
ExecStart=/home/torch/development/ncnn_opencv_server/workspace/ncnn_demo
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target