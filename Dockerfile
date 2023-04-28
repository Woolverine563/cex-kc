FROM ubuntu:20.04

RUN apt-get update && apt-get install -y git make python3-pip libreadline-dev libncurses5-dev libboost-program-options-dev
RUN pip install matplotlib scipy

RUN useradd -ms /bin/bash user
USER user
WORKDIR /home/user

COPY --chown=user . .
 
RUN cd dependencies/abc && make libabc.a -j4

ENTRYPOINT ["./test.sh"]