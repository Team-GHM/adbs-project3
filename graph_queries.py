"""import matplotlib.pyplot as plt

# Load the data from the output file
with open('read_ops_times_new.txt', 'r') as file:
    lines = file.readlines()

nops = []
ops_times = []

for line in lines:
    values = line.split()
    nops.append(int(values[0]))
    ops_times.append(float(values[1]))

# Create the time series graph
plt.figure(figsize=(10,8))
plt.plot(nops, ops_times, label='Average time taken by operations')
plt.xlabel('nops')
plt.ylabel('Average time taken for 100 operations (μs)')
plt.title('Time taken to perform nops')
plt.legend()
plt.grid()

#y_tick_positions = range(int(min(throughputs)), int(max(throughputs) + 1), 10000)  # Custom interval of 10000
#plt.yticks(y_tick_positions, [str(val) for val in y_tick_positions])

plt.savefig('/home/u1418793/adbs-project3/queries.png') 
plt.show()"""

import matplotlib.pyplot as plt

# Function to load data from a file
def load_data(file_path):
    nops = []
    ops_times = []

    with open(file_path, 'r') as file:
        lines = file.readlines()

    for line in lines:
        values = line.split()
        nops.append(int(values[0]))
        ops_times.append(float(values[1]))

    return nops, ops_times

# Load data for reads
nops_read_new, ops_times_read_new = load_data('read_ops_times_new.txt')
nops_read_old, ops_times_read_old = load_data('read_ops_times_old.txt')

# Load data for writes
nops_write_new, ops_times_write_new = load_data('write_ops_times_new.txt')
nops_write_old, ops_times_write_old = load_data('write_ops_times_old.txt')

# Create subplots for reads
plt.figure(figsize=(14, 8))

# Subplot for new reads
plt.subplot(2, 2, 1)
plt.plot(nops_read_new, ops_times_read_new, label='New Reads')
plt.xlabel('nops')
plt.ylabel('Average time taken for 100 operations (μs)')
plt.title('Time taken for New Reads')
plt.legend()
plt.grid()

# Subplot for old reads
plt.subplot(2, 2, 2)
plt.plot(nops_read_old, ops_times_read_old, label='Old Reads')
plt.xlabel('nops')
plt.ylabel('Average time taken for 100 operations (μs)')
plt.title('Time taken for Old Reads')
plt.legend()
plt.grid()

# Create subplots for writes
# Subplot for new writes
plt.subplot(2, 2, 3)
plt.plot(nops_write_new, ops_times_write_new, label='New Writes')
plt.xlabel('nops')
plt.ylabel('Average time taken for 100 operations (μs)')
plt.title('Time taken for New Writes')
plt.legend()
plt.grid()

# Subplot for old writes
plt.subplot(2, 2, 4)
plt.plot(nops_write_old, ops_times_write_old, label='Old Writes')
plt.xlabel('nops')
plt.ylabel('Average time taken for 100 operations (μs)')
plt.title('Time taken for Old Writes')
plt.legend()
plt.grid()

plt.tight_layout()  # Adjust layout to prevent overlap
plt.savefig('/home/u1418793/adbs-project3/operations_plots.png')
plt.show()
