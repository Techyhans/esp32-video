docker build -t espserver ./
docker run -p 8000:8000 -v /home/pi:/file -e "env_var_name=$1" espserver