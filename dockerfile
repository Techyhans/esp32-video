FROM arm32v7/python:3.7-slim-buster

RUN apt-get update \
&& apt-get install build-essential -y

COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

COPY ./rasp /app

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]