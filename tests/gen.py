# with open('my_file', 'wb') as f:
import random
import string


def get_key(val, my_dict):
    for key, value in my_dict.items():
        if val == value:
            return key

    return "key doesn't exist"


dict_size = dict()
dict_size['kB'] = 1024
dict_size['mB'] = 1024 * 1024
dict_size['gB'] = 1024 * 1024 * 1024
total_files = 4
steps = [1, 8, 64, 512]
max_size = 2 * 1024 * 1024 * 1024  # 2GB

for size in dict_size.values():
    for step in steps:
        file_size = step * size
        if file_size < max_size:
            f = open("rand_files/file_" + str(step) + "_" + get_key(size, dict_size) + ".txt", "w+")
            tf = int(file_size / 200)
            for __ in range(0, tf):
                f.write(random.choice(string.ascii_letters) * 200)
            f.close()
