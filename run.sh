docker build -t espserver ./
docker run -p 8000:8000 -v /home/pi:/file -e "DROPBOX_TOKEN=$1" espserver