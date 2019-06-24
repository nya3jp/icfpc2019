import argparse
import numpy as np
import random
import tensorflow as tf

from batchEnv import BatchEnvironment
from replayMemory import ReplayMemory, PriorityExperienceReplay
from model import create_deep_q_network, create_duel_q_network, create_model, create_distributional_model
from agent import DQNAgent

NUM_FRAME_PER_ACTION = 1
UPDATE_FREQUENCY = 1 # do one batch update when UPDATE_FREQUENCY number of new samples come
TARGET_UPDATE_FREQENCY = 10000
REPLAYMEMORY_SIZE = 500000
MAX_EPISODE_LENGTH = 100000
RMSP_EPSILON = 0.01
RMSP_DECAY = 0.95
RMSP_MOMENTUM =0.95
MAX_EPISODE_LENGTH = 100000
NUM_FIXED_SAMPLES = 10000
NUM_BURN_IN = 50000
LINEAR_DECAY_LENGTH = 4000000
NUM_EVALUATE_EPSIODE = 20

def show_numpy_pillow(np_image):
    return

def get_fixed_samples(env, num_actions, num_samples):
    fixed_samples = []
    num_environment = env.num_process
    env.reset()

    for _ in range(0, num_samples, num_environment):
        old_state, action, reward, new_state, is_terminal = env.get_state()
        action = np.random.randint(0, num_actions, size=(num_environment,))
        env.take_action(action)
        for state in new_state:
            fixed_samples.append(state)
    return np.array(fixed_samples)

def main():
    parser = argparse.ArgumentParser(description='Run DQN on Atari Space Invaders')
    parser.add_argument('--env', default='soundtype', help='Atari env name')
    parser.add_argument('--seed', default=10703, type=int, help='Random seed')
    #parser.add_argument('--input_shape', default=(84,84), help='Input shape')
    parser.add_argument('--gamma', default=0.99, help='Discount factor')
    parser.add_argument('--epsilon', default=0.1, help='Exploration probability in epsilon-greedy')
    parser.add_argument('--learning_rate', default=0.00025, help='Training learning rate.')
    parser.add_argument('--window_size', default=1, type = int, help=
                                'Number of frames to feed to the Q-network')
    parser.add_argument('--batch_size', default=32, type = int, help=
                                'Batch size of the training part')
    parser.add_argument('--num_process', default=3, type = int, help=
                                'Number of parallel environment')
    parser.add_argument('--num_iteration', default=20000000, type = int, help=
                                'number of iterations to train')
    parser.add_argument('--eval_every', default=0.001, type = float, help=
                                'What fraction of num_iteration to run between evaluations.')
    parser.add_argument('--is_duel', default=1, type = int, help=
                                'Whether use duel DQN, 0 means no, 1 means yes.')
    parser.add_argument('--is_double', default=1, type = int, help=
                                'Whether use double DQN, 0 means no, 1 means yes.')
    parser.add_argument('--is_per', default=1, type = int, help=
                                'Whether use PriorityExperienceReplay, 0 means no, 1 means yes.')
    parser.add_argument('--is_distributional', default=1, type = int, help=
                                'Whether use distributional DQN, 0 means no, 1 means yes.')
    parser.add_argument('--num_step', default=1, type = int, help=
                                'Num Step for multi-step DQN, 3 is recommended')
    parser.add_argument('--is_noisy', default=1, type = int, help=
                                'Whether use NoisyNet, 0 means no, 1 means yes.')


    args = parser.parse_args()
    #args.input_shape = tuple(args.input_shape)
    input_shape = (13,13)
    layer = 11
    collected = 4
    print('Environment: %s.'%(args.env,))
    num_actions = 11
    print('number_actions: %d.'%(num_actions,))

    random.seed(args.seed)
    np.random.seed(args.seed)
    tf.set_random_seed(args.seed)

    batch_environment = BatchEnvironment(args.env, args.num_process,
                layer, input_shape, NUM_FRAME_PER_ACTION, MAX_EPISODE_LENGTH)

    payload_total = layer * input_shape[0] * input_shape[1] + collected

    if args.is_per == 1:
        replay_memory = PriorityExperienceReplay(REPLAYMEMORY_SIZE, payload_total)
    else:
        replay_memory = ReplayMemory(REPLAYMEMORY_SIZE, payload_total)

    create_network_fn = create_deep_q_network if args.is_duel == 0 else create_duel_q_network
    create_model_fn = create_model if args.is_distributional == 0 else create_distributional_model
    noisy = True if args.is_noisy==1 else False
    online_model, online_params = create_model_fn(payload_total, num_actions,
                    'online_model', create_network_fn, trainable=True, noisy=noisy)
    target_model, target_params = create_model_fn(payload_total, num_actions,
                    'target_model', create_network_fn, trainable=False, noisy=noisy)
    update_target_params_ops = [t.assign(s) for s, t in zip(online_params, target_params)]


    agent = DQNAgent(online_model,
                    target_model,
                    replay_memory,
                    num_actions,
                    args.gamma,
                    UPDATE_FREQUENCY,
                    TARGET_UPDATE_FREQENCY,
                    update_target_params_ops,
                    args.batch_size,
                    args.is_double,
                    args.is_per,
                    args.is_distributional,
                    args.num_step,
                    args.is_noisy,
                    args.learning_rate,
                    RMSP_DECAY,
                    RMSP_MOMENTUM,
                    RMSP_EPSILON)

    #gpu_options = tf.GPUOptions(per_process_gpu_memory_fraction=0.4)
    #sess = tf.Session(config=tf.ConfigProto(gpu_options=gpu_options))
    sess = tf.Session(config=tf.ConfigProto())
    with sess.as_default():
        sess.run(tf.global_variables_initializer())
        # make target_model equal to online_model
        sess.run(update_target_params_ops)

        print('Prepare fixed samples for mean max Q.')
        fixed_samples = get_fixed_samples(batch_environment, num_actions, NUM_FIXED_SAMPLES)

        print('Burn in replay_memory.')
        agent.fit(sess, batch_environment, NUM_BURN_IN, do_train=False)
        
        # Begin to train:
        fit_iteration = int(args.num_iteration * args.eval_every)

        for i in range(0, args.num_iteration, fit_iteration):
            # Evaluate:
            reward_mean, reward_var = agent.evaluate(sess, batch_environment, NUM_EVALUATE_EPSIODE)
            mean_max_Q = agent.get_mean_max_Q(sess, fixed_samples)
            print("%d, %f, %f, %f"%(i, mean_max_Q, reward_mean, reward_var))
            # Train:
            agent.fit(sess, batch_environment, fit_iteration, do_train=True)

    batch_environment.close()

if __name__ == '__main__':
    main()
