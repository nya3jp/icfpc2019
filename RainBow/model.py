"""Q-networks"""

import tensorflow as tf
import numpy as np

# Returns tuple of flat output, flat output size, network_parameters.
def create_conv_network(input_frames, trainable):
    # input1: (batch size, 13, 13, 11)
    # input2: (4)
    nlayer = 11
    winsize = 13
    payload1 = winsize * winsize * nlayer
    payload2 = 4
    (input_frames_vis,
     input_frames_collected) = tf.split(input_frames, [payload1, payload2], 1)
    input_frames_collected_conv = tf.dtypes.cast(input_frames_collected, tf.float32)
    print("****shape", input_frames_collected_conv.shape)
    input_frames_2d = tf.reshape(input_frames_vis, [-1, winsize, winsize, nlayer])
    print(input_frames.shape)
    print(input_frames_2d.shape)

    # input1: (batch size, 13, 13, 11)
    conv1_W = tf.get_variable(shape=[3, 3, 11, 16], name='conv1_W',
        trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
    conv1_b = tf.Variable(tf.zeros([16], dtype=tf.float32),
        name='conv1_b', trainable=trainable)
    conv1 = tf.nn.conv2d(input_frames_2d, conv1_W, strides=[1, 1, 1, 1],
        padding='VALID', name='conv1')
    # (batch size, 11, 11, 16)
    output1 = tf.nn.relu(conv1 + conv1_b, name='output1')

    conv2_W = tf.get_variable(shape=[3, 3, 16, 32], name='conv2_W',
        trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
    conv2_b = tf.Variable(tf.zeros([32], dtype=tf.float32), name='conv2_b',
        dtype=tf.float32, trainable=trainable)
    conv2 = tf.nn.conv2d(output1, conv2_W, strides=[1, 1, 1, 1],
        padding='VALID', name='conv2')
    # (batch size, 9, 9, 32)
    output2 = tf.nn.relu(conv2 + conv2_b, name='output2')

    flat_output2_size = 9 * 9 * 32
    flat_output2 = tf.reshape(output2, [-1, flat_output2_size], name='flat_output2')
    
    print("$$debug: ", flat_output2.dtype)
    print("$$debug: ", flat_output2.shape)
    print("$$debug: ", input_frames_collected_conv.dtype)
    print("$$debug: ", input_frames_collected_conv.shape)

    return tf.concat([flat_output2, input_frames_collected_conv], 1), flat_output2_size + payload2, [conv1_W, conv1_b, conv2_W, conv2_b]

# Returns tuple of network, network_parameters.
def create_deep_q_network(input_frames, input_length, num_actions, trainable, noisy):
    flat_output, flat_output_size, parameter_list = create_conv_network(input_frames, trainable)

    if noisy == False:
        fc1_W = tf.get_variable(shape=[flat_output_size, 256], name='fc1_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fc1_b = tf.Variable(tf.zeros([256], dtype=tf.float32), name='fc1_b',
            trainable=trainable)
        # (batch size, 256)
        output3 = tf.nn.relu(tf.matmul(flat_output, fc1_W) + fc1_b, name='output3')

        fc2_W = tf.get_variable(shape=[256, num_actions], name='fc2_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fc2_b = tf.Variable(tf.zeros([num_actions], dtype=tf.float32), name='fc2_b',
            trainable=trainable)
        # (batch size, num_actions)
        q_network = tf.nn.relu(tf.matmul(output3, fc2_W) + fc2_b, name='q_network')

        parameter_list += [fc1_W, fc1_b, fc2_W, fc2_b]
    else:
        output3, parameter_list_output3 = noisy_dense(flat_output, name='noisy_fc1',
            input_size=flat_output_size, output_size=256, activation_fn=tf.nn.relu, trainable=trainable)
        q_network, parameter_list_q_network = noisy_dense(output3, name='noisy_fc2',
            input_size=256, output_size=num_actions, trainable=trainable)
        parameter_list += parameter_list_output3 + parameter_list_q_network
    return q_network, parameter_list


# Returns tuple of network, network_parameters.
def create_duel_q_network(input_frames, input_length, num_actions, trainable, noisy):
    flat_output, flat_output_size, parameter_list = create_conv_network(input_frames, trainable)

    if noisy==False:
        fcV_W = tf.get_variable(shape=[flat_output_size, 512], name='fcV_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fcV_b = tf.Variable(tf.zeros([512], dtype=tf.float32), name='fcV_b',
            dtype=tf.float32, trainable=trainable)
        outputV = tf.nn.relu(tf.matmul(flat_output, fcV_W) + fcV_b, name='outputV')

        fcV2_W = tf.get_variable(shape=[512, 1], name='fcV2_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fcV2_b = tf.Variable(tf.zeros([1], dtype=tf.float32), name='fcV2_b',
            trainable=trainable)
        outputV2 = tf.matmul(outputV, fcV2_W) + fcV2_b


        fcA_W = tf.get_variable(shape=[flat_output_size, 512], name='fcA_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fcA_b = tf.Variable(tf.zeros([512], dtype=tf.float32), name='fcA_b',
            trainable=trainable)
        outputA = tf.nn.relu(tf.matmul(flat_output, fcA_W) + fcA_b, name='outputA')

        fcA2_W = tf.get_variable(shape=[512, num_actions], name='fcA2_W',
            trainable=trainable, initializer=tf.contrib.layers.xavier_initializer())
        fcA2_b = tf.Variable(tf.zeros([num_actions], dtype=tf.float32), name='fcA2_b',
            trainable=trainable)
        outputA2 = tf.matmul(outputA, fcA2_W) + fcA2_b

        parameter_list += [fcV_W, fcV_b, fcV2_W, fcV2_b, fcA_W, fcA_b, fcA2_W, fcA2_b]
    else:
        outputV, parameter_list_outputV = noisy_dense(flat_output, name='fcV',
            input_size=flat_output_size, output_size=512, trainable=trainable, activation_fn=tf.nn.relu)
        outputV2, parameter_list_outputV2 = noisy_dense(outputV, name='fcV2',
            input_size=512, output_size=1, trainable=trainable)
        ouputA, parameter_list_outputA = noisy_dense(flat_output, name='fcA',
            input_size=flat_output_size, output_size=512, trainable=trainable, activation_fn=tf.nn.relu)
        outputA2, parameter_list_outputA2 = noisy_dense(ouputA, name='fcA2',
            input_size=512, output_size=num_actions, trainable=trainable)
        parameter_list += parameter_list_outputA+parameter_list_outputA2 +\
                          parameter_list_outputV+parameter_list_outputV2
    q_network = tf.nn.relu(outputV2 + outputA2 - tf.reduce_mean(outputA2), name='q_network')
    return q_network, parameter_list


def create_model(totsize, num_actions, model_name, create_network_fn, trainable, noisy):
    """Create the Q-network model."""
    with tf.variable_scope(model_name):
        input_frames = tf.placeholder(tf.float32, [None, totsize], name ='input_frames')
        input_length = totsize
        q_network, parameter_list = create_network_fn(
            input_frames, input_length, num_actions, trainable, noisy)

        mean_max_Q = tf.reduce_mean( tf.reduce_max(q_network, axis=[1]), name='mean_max_Q')
        action = tf.argmax(q_network, axis=1)

        model = {
            'q_values': q_network,
            'input_frames': input_frames,
            'mean_max_Q': mean_max_Q,
            'action': action,
        }
    return model, parameter_list

def create_distributional_model(totsize, num_actions,
                model_name, create_network_fn, trainable, noisy):
    N_atoms = 51
    V_Max = 20.0
    V_Min = 0.0
    Delta_z = (V_Max - V_Min)/(N_atoms - 1)
    z_list = tf.constant([V_Min + i * Delta_z for i in range(N_atoms)],dtype=tf.float32)
    z_list_broadcasted = tf.tile(tf.reshape(z_list,[1,N_atoms]), tf.constant([num_actions,1]))

    """Create the Q-network model."""
    with tf.variable_scope(model_name):
        input_frames = tf.placeholder(tf.float32, [None, totsize], name ='input_frames')
        input_length = totsize
        q_distributional_network, parameter_list = create_network_fn(
            input_frames, input_length, num_actions*N_atoms, trainable, noisy)
        q_distributional_network = tf.reshape(q_distributional_network, [-1, num_actions, N_atoms])
        # batch_size * num_actions * N_atoms
        q_distributional_network = tf.nn.softmax(q_distributional_network, dim = 2)
        # Clipping to prevent NaN
        q_distributional_network = tf.clip_by_value(q_distributional_network, 1e-8, 1.0-1e-8)

        # get q_network by expectation of q_distributional_network
        q_network =  tf.multiply(q_distributional_network, z_list_broadcasted)
        q_network = tf.reduce_sum(q_network, axis=2, name='q_values')
        mean_max_Q = tf.reduce_mean( tf.reduce_max(q_network, axis=[1]), name='mean_max_Q')
        action = tf.argmax(q_network, axis=1)

        model = {
            'q_distributional_network': q_distributional_network,
            'q_values': q_network,
            'input_frames': input_frames,
            'mean_max_Q': mean_max_Q,
            'action': action,
        }
    return model, parameter_list

def noisy_dense(x, input_size, output_size, name, trainable, activation_fn=tf.identity):

    # the function used in eq.7,8
    def f(x):
        return tf.multiply(tf.sign(x), tf.pow(tf.abs(x), 0.5))
    # Initializer of \mu and \sigma
    mu_init = tf.random_uniform_initializer(minval=-1*1/np.power(input_size, 0.5),
                                                maxval=1*1/np.power(input_size, 0.5))
    sigma_init = tf.constant_initializer(0.4/np.power(input_size, 0.5))
    # Sample noise from gaussian
    p = tf.random_normal([input_size, 1])
    q = tf.random_normal([1, output_size])
    f_p = f(p); f_q = f(q)
    w_epsilon = f_p*f_q; b_epsilon = tf.squeeze(f_q)

    # w = w_mu + w_sigma*w_epsilon
    w_mu = tf.get_variable(name + "/w_mu", [input_size, output_size],
            initializer=mu_init, trainable=trainable)
    w_sigma = tf.get_variable(name + "/w_sigma", [input_size, output_size],
            initializer=sigma_init, trainable=trainable)
    w = w_mu + tf.multiply(w_sigma, w_epsilon)
    ret = tf.matmul(x, w)

    # b = b_mu + b_sigma*b_epsilon
    b_mu = tf.get_variable(name + "/b_mu", [output_size],
            initializer=mu_init, trainable=trainable)
    b_sigma = tf.get_variable(name + "/b_sigma", [output_size],
            initializer=sigma_init, trainable=trainable)
    b = b_mu + tf.multiply(b_sigma, b_epsilon)
    return activation_fn(ret + b), [w_mu, w_sigma, b_mu, b_sigma]

