# Description
This repo is aimed to create a esp32 video recorder, then upload the recorded video to dropbox through a http.

# Component
In order to achieve the objective, a raspberry pi is required as an intermediate server due to memory limit of esp32.

## Steps
1. Esp32 will record a video, then save to sdcard.
2. Next, esp32 will send the file to raspberry pi server via ftp.
3. Upon send the file via ftp, esp32 will send a post request to raspberry pi to trigger the file sending to dropbox server.

## UI
![](metadata/1.png)

## Dropbox setup
1. follow this [link](https://docs.gravityforms.com/creating-a-custom-dropbox-app/) to create a custom app in dropbox
2. follow this [link](https://dropbox.tech/developers/generate-an-access-token-for-your-own-account) to generate the access token for dropbox

## Arduino setup
1. Fill up the variables which are left  `*****` in [here](https://github.com/Techyhans/esp32-video/blob/v3/arduino/esp32/esp32.ino#L88-L90)
   1. Wifi ssid
   2. Wifi password
   3. ftp server ip (use `ifconfig` to determine)
   4. ftp user
   5. ftp password

## Raspberry Pi setup
1. follow this [link](https://www.youtube.com/watch?v=Ywx8lmT_6is) to setup ftp server in raspberry pi
2. follow this [link](https://phoenixnap.com/kb/docker-on-raspberry-pi) to install docker in raspberry pi (it might take some time)
3. After docker is installed, in this directory, run command `sudo docker build -t espserver ./` to build the docker image
4. Run the command `sudo docker run -p 8000:8000 -v {{RASPBERRY_PI_HOME_DIR}}:/file -e "DROPBOX_TOKEN={{ENTER_YOUR_DROPBOX_TOKEN}}" espserver` by replacing the variable with `{{}}`
