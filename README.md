# Description
This repo is aimed to create a esp32 video recorder, then upload the recorded video to dropbox through a http.

# Component
In order to achieve the objective, a raspberry pi is required as an intermediate server due to memory limit of esp32.

## Steps
1. Esp32 will record a video, then save to sdcard.
2. Next, esp32 will send the file to raspberry pi server via ftp.
3. Upon sendinconda install -c anaconda dropboxg the file via ftp, esp32 will send a post request to raspberry pi to trigger the file sending to dropbox server.