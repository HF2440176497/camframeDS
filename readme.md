# 线扫相机说明



```
度申线扫相机SDK封装。..
```
防火墙设置：

sudo ufw allow ssh

sudo iptables -D INPUT -i ens3f0 -s 192.168.1.0/24 -j ACCEPT

sudo iptables -D FORWARD -i ens3f0 -s 192.168.1.0/24 -j ACCEPT

sudo iptables -A INPUT -i ens3f0 -s 192.168.1.0/24 -j ACCEPT

sudo iptables -A FORWARD -i ens3f0 -s 192.168.1.0/24 -j ACCEPT

sudo iptables -S

iptables -A INPUT -p tcp --dport 5901 -j ACCEPT
ufw allow from 192.168.1.0/24

# 1.配置环境变量
export CAM_DS_LIB_PATH=/kvm/do3think/DVP2-Linux64
# 2.运行
python3 example/example.py