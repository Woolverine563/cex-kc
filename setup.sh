sudo apt-get update 
sudo apt-get install -y python3-pip libreadline-dev libncurses5-dev libboost-program-options-dev
pip3 install matplotlib scipy
git submodule update --init
cd dependencies/abc
make libabc.a -j $(nproc)