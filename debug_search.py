
filename = "data/AAPL_2012-06-21_34200000_57600000_message_10.csv"
target_price = "5854000"
target_id = "13419503" # From line 448 check

print(f"Scanning {filename} for {target_price}...")

with open(filename, 'r') as f:
    for i, line in enumerate(f):
        if i >= 450: break
        if target_price in line:
            print(f"{i+1}: {line.strip()}")
