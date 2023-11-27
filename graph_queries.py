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
plt.figure(figsize=(10, 6))

plt.plot(nops_read_new, ops_times_read_new, color='red', label='Tunable Reads')
plt.plot(nops_read_old, ops_times_read_old, color='blue', label='NonTunable Reads') 
plt.plot(nops_write_new, ops_times_write_new, color='green', label='Tunable Writes')
plt.plot(nops_write_old, ops_times_write_old, color='orange', label='NonTunable Writes') 

plt.legend()
plt.xlabel('nops') 
plt.ylabel('Average time (μs)')
plt.title('Operation Time')

plt.tight_layout()
plt.savefig('/home/u1418793/adbs-project3/operations_plots.png') 
plt.show()
