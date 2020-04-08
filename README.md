# Dice game 

## Build Status

Branch|Build Status
---|---
Master|[![master](https://travis-ci.org/DaoCasino/dice-game.svg?branch=master)](https://travis-ci.org/DaoCasino/dice-game.svg?branch=master)

## Description
Simple dice game for DAOPlatform based on DAOBet blockchain. 
Implemeted by [sdk](https://github.com/DaoCasino/game-contract-sdk). 

## Rules
Player can choose value from 1 to 99. 
After throwing the dice, if the user value is less than the random value of the dice, the player win. Win coefficient calculating by formula:
<img src="https://render.githubusercontent.com/render/math?math=(100 * (1 - he))/(100 - N)"> where `he` - house edge and `N` - player number.

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
