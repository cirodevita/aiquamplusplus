#!/bin/bash

start_date=""
base_url=""
output_folder=""

# Check if the input date format is valid
if [[ ! $start_date =~ ^[0-9]{8}Z[0-9]{4}$ ]]; then
    echo "Invalid date format. Please use 'YYYYMMDDZHH'."
    exit 1
fi

# Convert input date to human-readable format for date command
start_date_formatted=$(echo "$start_date" | sed -e 's/\(....\)\(..\)\(..\)Z\(..\)/\1-\2-\3 \4:00:00/')

# Convert input date to epoch time
input_epoch=$(date -u -d "$start_date_formatted" +"%s")

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
    previous_date=$(date -u -d "@$previous_epoch" +"%Y%m%dZ%H")

    # Extract year, month, day, and hour
    year=$(date -u -d "@$previous_epoch" +"%Y")
    month=$(date -u -d "@$previous_epoch" +"%m")
    day=$(date -u -d "@$previous_epoch" +"%d")
    hour=$(date -u -d "@$previous_epoch" +"%H")

    # Construct the file URL
    file_url="${base_url}/${year}/${month}/${day}/wcm3_d03_${previous_date}00.nc"

    # echo \"wcm3_d03_${previous_date}00.nc\",
    
    # Download the file
    wget -P "$output_folder" "$file_url"
done