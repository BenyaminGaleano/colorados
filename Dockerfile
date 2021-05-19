from ubuntu 16:04

workdir /root/pintos

run apt update
run apt install -y wget python3 build-essential qemu qemu-kvm git gdb
run ln -s `which python3` /usr/bin/python
run bash -c "$(wget https://raw.githubusercontent.com/ohmybash/oh-my-bash/master/tools/install.sh -O -)"
run echo "export PATH=\$PATH:~/pintos/utils" >> /root/.bashrc
run echo "PINTOSPH=\"threads\" # stop@000001" >> /root/.bashrc
run echo "export PINTOSPH" >> ~/.bashrc
run mkdir /root/.pintos-docker
run mv /root/.bashrc /root/.save.bashrc
run echo "#!/bin/bash\ncp /root/.save.bashrc /root/.pintos-docker/.bashrc\nsource /root/.bashrc" > /bin/bash-config
run chmod +x /bin/bash-config
run ln -s /root/.pintos-docker/.bashrc /root/.bashrc

