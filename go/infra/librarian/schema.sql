USE ts;

CREATE TABLE solutions (
    `id` INT(10) PRIMARY KEY AUTO_INCREMENT,
    `solver` VARCHAR(64) NOT NULL,
    `problem` VARCHAR(64) NOT NULL,
    `purchase` VARCHAR(64) NOT NULL DEFAULT '',
    `evaluator` VARCHAR(64) NOT NULL,
    `solution_hash` VARCHAR(64) NOT NULL,
    `score` INT(10) NOT NULL,
    `valid` BOOL NOT NULL DEFAULT TRUE,
    `submitted` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (solver, problem, purchase, solution_hash)
) ENGINE=InnoDB DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
