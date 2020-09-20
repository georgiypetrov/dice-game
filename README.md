# Dice game 

## Build Status

Branch|Build Status
---|---
Master|[![master](https://travis-ci.org/georgiypetrov/dice-game.svg?branch=master)](https://travis-ci.org/georgiypetrov/dice-game.svg?branch=master)

## Description
Modified dice (range dice) game for DAOPlatform based on DAOBet blockchain. 
Implemented by [sdk](https://github.com/DaoCasino/game-contract-sdk). 

## Rules
Player can choose two values, first(small) from 1 to 99, second(big) from 2 to 100. 
After throwing the dice, if the random value of the dice is equal or greater than player small value and less than player big value, the player win,
i.e. <img src="https://render.githubusercontent.com/render/math?math=N_{small} \leq N_{rnd} <  N_{big}"> 
where <img src="https://render.githubusercontent.com/render/math?math=N_{rnd}"> - random dice value, 
<img src="https://render.githubusercontent.com/render/math?math=N_{small}"> - player small value
and <img src="https://render.githubusercontent.com/render/math?math=N_{big}"> - player big value.
Win coefficient calculating by formula:
<img src="https://render.githubusercontent.com/render/math?math=(100 * (1 - he))/(100 - (N_{big} - N_{small}))"> 
where <img src="https://render.githubusercontent.com/render/math?math=he"> - house edge.

# Try it

## Build
```bash
git clone https://github.com/DaoCasino/dice-game
cd dice-game
git submodule init
git submodule update --init --recursive
./cicd/run build
```
## Run unit tests
```bash
./cicd/run test
```
