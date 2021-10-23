FROM tiangolo/uvicorn-gunicorn:python3.7

COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

COPY ./rasp /app

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "80"]