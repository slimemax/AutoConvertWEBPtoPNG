# WebP to PNG Converter

This project is a multi-threaded C program that monitors a directory for new `.webp` files, converts them to `.png` format, and logs the processed files. 

## Features
- Monitors a specified directory for new `.webp` files.
- Converts `.webp` files to `.png` format.
- Uses multi-threading to process multiple files concurrently.
- Logs processed files to avoid duplicate processing.

## Requirements
- C compiler (e.g., `gcc`)
- libwebp
- libpng
- pthreads library

## Installation

### Install Dependencies

#### On Ubuntu/Debian
```sh
sudo apt update
sudo apt install build-essential libwebp-dev libpng-dev
```

#### On CentOS/RHEL
```sh
sudo yum groupinstall "Development Tools"
sudo yum install libwebp-devel libpng-devel
```

### Compile the Program

1. Clone the repository:
    ```sh
    git clone <repository-url>
    cd <repository-directory>
    ```

2. Compile the source code:
    ```sh
    gcc -o convert_webp convert_webp.c -lpthread -lwebp -lpng
    ```

## Usage

1. Run the program:
    ```sh
    ./convert_webp
    ```

2. The program will monitor the current directory for new `.webp` files and convert them to `.png` format, saving the output in the `Dalle3Pngs` directory.

## Handling Signals
- The program handles `SIGINT` (Ctrl+C) to gracefully stop monitoring and clean up resources.

## Additional Information

- The program logs processed files in `processed_files.log`.
- It creates the `Dalle3Pngs` directory if it doesn't exist.
- Processes up to 4 files concurrently using threads.

## License

This project is licensed under the MIT License.

---
