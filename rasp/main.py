from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

import dropbox
from dropbox.files import WriteMode
from dropbox.exceptions import ApiError, AuthError

"""
Enter you token here...
See <https://blogs.dropbox.com/developers/2014/05/generate-an-access-token-for-your-own-account/>
"""
TOKEN = 'FyIOnxGNo_4AAAAAAAAAASPHz9w7Dcx-pnGmUl7E5wS8I772N0fnyq-5M66XADNl'

app = FastAPI()


class UploadRequest(BaseModel):
    filePath: str
    targetPath: str


@app.get("/")
async def root():
    return {"message": "Connected Successfully!"}


@app.post("/upload")
def upload(req: UploadRequest):
    print("Connecting to dropbox...")
    with dropbox.Dropbox(TOKEN) as dbx:

        # Check that the access token is valid
        try:
            dbx.users_get_current_account()
        except AuthError:
            raise HTTPException(status_code=400, detail="Unable to connect to dropbox")

        path = req.filePath
        with open(path, 'rb') as f:
            print("Uploading " + path + " to Dropbox ...")

            try:
                dbx.files_upload(f.read(), req.targetPath, mode=WriteMode("overwrite"))
            except ApiError as err:
                # This checks for the specific error where a user doesn't have
                # enough Dropbox space quota to upload this file
                if (err.error.is_path() and
                        err.error.get_path().reason.is_insufficient_space()):
                    raise HTTPException(status_code=400, detail="ERROR: Cannot back up; insufficient space.")
                elif err.user_message_text:
                    print(err.user_message_text)
                    raise HTTPException(status_code=400, detail=err.user_message_text)
                else:
                    print(err)
                    raise HTTPException(status_code=400, detail=err)

        return {"message": "uploaded"}
