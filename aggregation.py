import pandas as pd
import json
import os

def aggregate_data(input_file):
    # Load the data from the JSON file
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    # Filter out already aggregated data
    non_aggregated_data = [entry for entry in data if not entry.get('aggregiert', False)]
    
    # If no new data to aggregate, stop the process
    if not non_aggregated_data:
        print(f"No new data to aggregate in {input_file}.")
        return
    
    # Convert non-aggregated data to DataFrame
    df = pd.DataFrame(non_aggregated_data)
    
    # Convert the timestamp to datetime
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Set timestamp as index for easier aggregation
    df.set_index('timestamp', inplace=True)
    
    # Aggregating data by hourly mean, excluding non-numeric columns like 'location'
    df_agg = df.resample('h').mean(numeric_only=True)
    
    # Remove rows with NaN values
    df_agg.dropna(inplace=True)
    
    # Reset the index to get 'timestamp' back as a column
    df_agg.reset_index(inplace=True)
    
    # Convert the timestamp to ISO 8601 format to match the input format
    df_agg['timestamp'] = df_agg['timestamp'].dt.strftime('%Y-%m-%dT%H:%M:%S%z')
    
    # Add the "aggregiert" attribute to each record
    df_agg['aggregiert'] = True
    
    # Prepare the aggregated data in the same format
    aggregated_data = df_agg.to_dict(orient='records')
    
    # Combine already aggregated data with newly aggregated data
    combined_data = [entry for entry in data if entry.get('aggregiert', False)] + aggregated_data
    
    # Overwrite the original file with the updated data
    with open(input_file, 'w') as f:
        json.dump(combined_data, f, indent=4)
    
    print(f"Aggregation completed for {input_file}")

def process_directory(directory):
    # Iterate over all files in the directory
    for filename in os.listdir(directory):
        # Only process JSON files
        if filename.endswith('.json'):
            file_path = os.path.join(directory, filename)
            print(f"Processing file: {file_path}")
            aggregate_data(file_path)

# Example usage
directory = '/home/pi/plant-monitoring-system/data'
process_directory(directory)
