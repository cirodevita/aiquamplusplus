#!/bin/bash

start_date=""
base_url=""
output_folder="inputs/"

# Check if the input date format is valid
if [[ ! $start_date =~ ^[0-9]{8}Z[0-9]{4}$ ]]; then
    echo "Invalid date format. Please use 'YYYYMMDDZHH'."
    exit 1
fi

# Convert input date to human-readable format for date command
start_date_formatted=$(echo "$start_date" | sed -e 's/\(....\)\(..\)\(..\)Z\(..\)/\1-\2-\3 \4:00:00/')

# Determine the operating system
os_type=$(uname)

# Convert input date to epoch time based on the OS
if [[ "$os_type" == "Darwin" ]]; then
    # macOS
    input_epoch=$(date -u -j -f "%Y-%m-%d %H:%M:%S" "$start_date_formatted" +"%s")
else
    # Linux
    input_epoch=$(date -u -d "$start_date_formatted" +"%s")
fi

# Check if the input date is valid
if [ $? -ne 0 ]; then
    echo "Invalid date format. Please use 'YYYYMMDDZHH'."
    exit 1
fi

# Print the previous 73 hours
for ((i=0; i<=73; i++)); do
    # Calculate the epoch time for each hour
    previous_epoch=$((input_epoch - (i * 3600)))
    # Convert the epoch time back to human-readable format
    if [[ "$os_type" == "Darwin" ]]; then
        # macOS
        previous_date=$(date -u -r "$previous_epoch" +"%Y%m%dZ%H")
        year=$(date -u -r "$previous_epoch" +"%Y")
        month=$(date -u -r "$previous_epoch" +"%m")
        day=$(date -u -r "$previous_epoch" +"%d")
        hour=$(date -u -r "$previous_epoch" +"%H")
    else
        # Linux
        previous_date=$(date -u -d "@$previous_epoch" +"%Y%m%dZ%H")
        year=$(date -u -d "@$previous_epoch" +"%Y")
        month=$(date -u -d "@$previous_epoch" +"%m")
        day=$(date -u -d "@$previous_epoch" +"%d")
        hour=$(date -u -d "@$previous_epoch" +"%H")
    fi

    # Construct the file URL
    file_url="${base_url}/${year}/${month}/${day}/wcm3_d03_${previous_date}00.nc"

    # echo \"wcm3_d03_${previous_date}00.nc\",
    
    # Construct the file name
    file_name="wcm3_d03_${previous_date}00.nc"
    
    # Check if the file already exists
    if [[ -f "${output_folder}/${file_name}" ]]; then
        echo "File ${file_name} already exists. Skipping download."
    else
        # Download the file if it doesn't exist
        wget -P "$output_folder" "$file_url"
    fi
done