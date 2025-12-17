
path = 'data/AAPL_2012-06-21_34200000_57600000_message_10.csv'
price = '5854000'
with open(path) as f:
    for i, line in enumerate(f):
         if i > 500: break
         if price in line:
             print(f"Line {i+1}: {line.strip()}")
