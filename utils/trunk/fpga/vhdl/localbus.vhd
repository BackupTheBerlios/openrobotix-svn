--------------------------------------------------------------------------------
-- localbus.vhd - Localbus FPGA design of Miniroboter CPU-Board
--
-- Copyright (C) 2010
-- Heinz Nixdorf Institute - University of Paderborn
-- Department of System and Circuit Technology
-- Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>
--
-- This library is free software; you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published
-- by the Free Software Foundation; either version 3 of the License, or
-- (at your option) any later version.
--
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library. If not, see <http://www.gnu.org/licenses/>
--------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;

entity localbus is
	port (
		CLK			: in	std_logic;
		RESET_N			: in	std_logic;

		MEM_ADDR		: in	std_logic_vector(25 downto 2);
		MEM_DATA_I		: in	std_logic_vector(31 downto 0);
		MEM_DATA_O		: out	std_logic_vector(31 downto 0);
		MEM_DATA_T_N		: out	std_logic_vector(31 downto 0);
		MEM_DQM			: in	std_logic_vector(3 downto 0);
		MEM_CS_N		: in	std_logic;
		MEM_OE_N		: in	std_logic;
		MEM_PWE_N		: in	std_logic;

		LOC_ADDR		: out	std_logic_vector(25 downto 2);
		LOC_DATA_I		: in	std_logic_vector(31 downto 0);
		LOC_DATA_O		: out	std_logic_vector(31 downto 0);
		LOC_DQM			: out	std_logic_vector(3 downto 0);
		LOC_VALID_N		: out	std_logic;
		LOC_READ_N		: out	std_logic;
		LOC_WRITE_N		: out	std_logic
	);
end localbus;

architecture Behavioral of localbus is

	type STATE_TYPE is (IDLE, READ_START, READ_DATA, READ_TRANS, READ_TRANS2, READ_WAIT,
				  WRITE_START, WRITE_DATA, WRITE_WAIT);

	signal current_state, next_state : STATE_TYPE;

	signal next_MEM_ADDR  		: std_logic_vector(25 downto 2);
	signal current_MEM_ADDR		: std_logic_vector(25 downto 2);
	signal next_MEM_DATA_IN		: std_logic_vector(31 downto 0);
	signal current_MEM_DATA_IN	: std_logic_vector(31 downto 0);
	signal next_MEM_DATA_OUT	: std_logic_vector(31 downto 0);
	signal current_MEM_DATA_OUT	: std_logic_vector(31 downto 0);
	signal next_MEM_DATA_T_N	: std_logic_vector(31 downto 0);
	signal current_MEM_DATA_T_N	: std_logic_vector(31 downto 0);
	signal next_MEM_DQM  		: std_logic_vector(3 downto 0);
	signal current_MEM_DQM		: std_logic_vector(3 downto 0);
	signal next_MEM_CS_N		: std_logic;
	signal current_MEM_CS_N		: std_logic;
	signal next_MEM_OE_N		: std_logic;
	signal current_MEM_OE_N		: std_logic;
	signal next_MEM_PWE_N		: std_logic;
	signal current_MEM_PWE_N		: std_logic;
	signal read_flag_n		: std_logic;

begin

	next_MEM_ADDR <= MEM_ADDR;
	next_MEM_DATA_IN <= MEM_DATA_I;
	next_MEM_DATA_OUT <= LOC_DATA_I;
	next_MEM_DQM <= MEM_DQM;
	next_MEM_CS_N <= MEM_CS_N;
	next_MEM_OE_N <= MEM_OE_N;
	next_MEM_PWE_N <= MEM_PWE_N;
	
	MEM_DATA_O <= current_MEM_DATA_OUT;
	MEM_DATA_T_N <= current_MEM_DATA_T_N;

	INTERFACE_ACCESS: process(current_state, current_MEM_CS_N, current_MEM_OE_N, current_MEM_PWE_N,
					  current_MEM_ADDR, current_MEM_DQM, current_MEM_DATA_IN)
	begin
		LOC_ADDR <= (others => '0');
		LOC_DATA_O <=  (others => '0');
		LOC_DQM <= (others => '0');
		LOC_VALID_N <= '1';
		LOC_READ_N <= '1';
		LOC_WRITE_N <= '1';
		read_flag_n <= '1';
		next_MEM_DATA_T_N <= (others => '1');
		next_state <= CURRENT_STATE;
		case CURRENT_STATE is
			when IDLE => 
				if (current_MEM_CS_N = '0') then
					if (current_MEM_OE_N = '0') then	-- Read
						next_state <= READ_START;
					elsif (current_MEM_PWE_N = '0') then -- Write
						next_state <= WRITE_START;
					end if;
				end if;

			when READ_START =>
				LOC_ADDR <= current_MEM_ADDR;
				LOC_DQM <= current_MEM_DQM;
				LOC_VALID_N <= '0';
				LOC_READ_N <= '0';
				next_state <= READ_DATA;

			when READ_DATA =>
				LOC_ADDR <= current_MEM_ADDR;
				LOC_DQM <= current_MEM_DQM;
				next_MEM_DATA_T_N <= (others => '0');
				read_flag_n <= '0';
				LOC_VALID_N <= '0';
				next_state <= READ_TRANS;

			when READ_TRANS =>
				next_MEM_DATA_T_N <= (others => '0');
				next_state <= READ_TRANS2;

			when READ_TRANS2 =>
				next_MEM_DATA_T_N <= (others => '0');
				next_state <= READ_WAIT;

			when READ_WAIT =>
				if (current_MEM_OE_N = '1') then
					next_state <= IDLE;
				end if;

			when WRITE_START =>
				LOC_ADDR <= current_MEM_ADDR;
				LOC_DQM <= current_MEM_DQM;				
				LOC_VALID_N <= '0';
				next_state <= WRITE_DATA;

			when WRITE_DATA =>
				LOC_ADDR <= current_MEM_ADDR;
				LOC_DQM <= current_MEM_DQM;				
				LOC_DATA_O <= current_MEM_DATA_IN;
				LOC_VALID_N <= '0';
				LOC_WRITE_N <= '0';
				next_state <= WRITE_WAIT;

			when WRITE_WAIT =>
				if (current_MEM_PWE_N = '1') then
					next_state <= IDLE;
				end if;
		end case;
	end process INTERFACE_ACCESS;
	
	REGISTERS: process(CLK, RESET_N)
	begin
		if (RESET_N = '0') then
			current_state <= IDLE;
			current_MEM_ADDR <= (others => '0');
			current_MEM_DATA_OUT <= (others => '0');
			current_MEM_DATA_IN <= (others => '0');
			current_MEM_DATA_T_N <= (others => '1');
			current_MEM_DQM <= (others => '0');
			current_MEM_CS_N <= '1';
			current_MEM_OE_N <= '1';
			current_MEM_PWE_N <= '1';
		elsif (CLK'event and CLK = '1') then
			current_MEM_ADDR <= next_MEM_ADDR;
			if (read_flag_n = '0') then
				current_MEM_DATA_OUT <= next_MEM_DATA_OUT;
			end if;
			current_MEM_DATA_IN <= next_MEM_DATA_IN;
			current_MEM_DATA_T_N <= next_MEM_DATA_T_N;
			current_MEM_DQM <= next_MEM_DQM;
			current_MEM_CS_N <= next_MEM_CS_N;
			current_MEM_OE_N <= next_MEM_OE_N;
			current_MEM_PWE_N <= next_MEM_PWE_N;
			current_state <= next_state;
		end if;
	end process REGISTERS;
end Behavioral;
