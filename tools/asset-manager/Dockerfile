FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends openssh-client && rm -rf /var/lib/apt/lists/*
# Pillow splits and aligns multi-view turnaround sheets (split_sheet)
RUN pip install --no-cache-dir pillow==11.3.0
WORKDIR /app
COPY app.py creature.py .
COPY static static
COPY motion motion
COPY body-depth body-depth
ENV ASSET_ROOT=/data/assets DATA_DIR=/data SSH_KEY=/ssh/id_ed25519 PORT=8090 PYTHONUNBUFFERED=1
RUN useradd -u 1000 -m app
USER app
EXPOSE 8090
CMD ["python", "app.py"]
