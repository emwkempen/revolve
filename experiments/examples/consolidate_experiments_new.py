import os
import csv
import json

# set these variables according to your experiments #
dirpath = 'data'
experiments_type = [
                    'test_4'
                    ]
runs = 1
# set these variables according to your experiments #


def build_headers(path):

    print(path + "/all_measures.txt")
    file_summary = open(path + "/all_measures.tsv", "w+")
    file_summary.write('robot_id\t')

    behavior_headers = []
    with open(path + '/data_fullevolution/descriptors/behavioural/behavior_desc_1.txt') as file:
    # with open(path + '/data_fullevolution/descriptors/behavior_desc_robot_1.txt') as file:
        for line in file:
            measure, value = line.strip().split(' ')
            behavior_headers.append(measure)
            file_summary.write(measure+'\t')

    phenotype_headers = []
    with open(path + '/data_fullevolution/descriptors/phenotype_desc_1.txt') as file:
    # with open(path + '/data_fullevolution/descriptors/phenotype_desc_robot_1.txt') as file:
        for line in file:
            measure, value = line.strip().split(' ')
            phenotype_headers.append(measure)
            file_summary.write(measure+'\t')
            # if measure == "bottom_layer":
            #     break
    file_summary.write('fitness\n')
    file_summary.close()

    file_summary = open(path + "/snapshots_ids.tsv", "w+")
    file_summary.write('generation\trobot_id\n')
    file_summary.close()

    return behavior_headers, phenotype_headers


for exp in experiments_type:
    for run in range(1, runs+1):

        print(exp, run)
        path = os.path.join(dirpath, str(exp), str(run))
        behavior_headers, phenotype_headers = build_headers(path)

        file_summary = open(path + "/all_measures.tsv", "a")
        # working with csv instead of directory containing .txt files
        with open(path+'/data_fullevolution/fitness.csv', 'r') as fitness_csv:
            fitness_file = csv.reader(fitness_csv)
            for fitness_row in fitness_file:
                print(fitness_row[-1])
                robot_id = fitness_row[0]
                file_summary.write(robot_id+'\t')

                bh_file = path+'/data_fullevolution/descriptors/behavioural/behavior_desc_'+robot_id+'.txt'
                if os.path.isfile(bh_file):
                    with open(bh_file) as file:
                        for line in file:
                            data = line.strip().split(' ')
                            value = data[-1]
                            file_summary.write(value + '\t')
                else:
                    for h in behavior_headers:
                        file_summary.write('None'+'\t')

                pt_file = path+'/data_fullevolution/descriptors/phenotype_desc_'+robot_id+'.txt'
                if os.path.isfile(pt_file):
                    with open(pt_file) as file:
                        for line in file:
                            data = line.strip().split(' ')
                            value = data[-1]
                            file_summary.write(value+'\t')
                else:
                    for h in phenotype_headers:
                        file_summary.write('None'+'\t')

                fitness = fitness_row[-1]
                file_summary.write(fitness + '\n')
        file_summary.close()

        file_summary = open(path + "/snapshots_ids.tsv", "a")
        for r, d, f in os.walk(path):
            for dir in d:
                if 'selectedpop' in dir:
                    gen = dir.split('_')[1]
                    for r2, d2, f2 in os.walk(path + '/selectedpop_' + str(gen)):
                        for file in f2:
                            if 'body' in file:
                                id = file.split('.')[0].split('_')[-1]
                                file_summary.write(gen+'\t'+id+'\n')
        file_summary.close()
