import random
from scipy.stats import zipf

def generate_random_keys(number_of_distinct_keys, nops):
    with open("random_keys.txt", "w") as file:
        for _ in range(nops):
            key = random.randint(0, number_of_distinct_keys - 1)
            value = str(key) + ":"
            file.write(f"{key} {value}\n")

def generate_skewed_keys(number_of_distinct_keys, nops, skewness=2.0):
    zipf_dist = zipf(a=skewness)

    with open("skewed_keys.txt", "w") as file:
        for _ in range(nops):
            key = zipf_dist.rvs(size=1)[0] % number_of_distinct_keys
            value = str(key) + ":"
            file.write(f"{key} {value}\n")

if __name__ == "__main__":
    number_of_distinct_keys = 2 ** 10  # Set to the desired number of distinct keys
    nops = 2 ** 12  # Set to the desired number of operations
    skewness = 2.0  # Adjust the skewness parameter as needed

    generate_random_keys(number_of_distinct_keys, nops)
    generate_skewed_keys(number_of_distinct_keys, nops, skewness)
