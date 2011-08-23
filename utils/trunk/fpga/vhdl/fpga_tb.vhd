--------------------------------------------------------------------------------
-- fpga_tb.vhd - Testbed FPGA design of Miniroboter CPU-Board
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
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.std_logic_unsigned.all;
USE ieee.numeric_std.ALL;

ENTITY FPGA_TB IS
END FPGA_TB;

ARCHITECTURE behavior OF FPGA_TB IS

	-- Component Declaration for the Unit Under Test (UUT)
	component FPGA_TOP is
	port (
		SDCLK0			: in	std_logic;
		FPGA_RESET_N 		: in	std_logic;
		DATA			: inout	std_logic_vector(31 downto 0);
		ADDR			: in	std_logic_vector(25 downto 2);
		DQM			: in	std_logic_vector(3 downto 0);
		OE_N			: in	std_logic;
		PWE_N			: in	std_logic;
--		RDWR_N			: in	std_logic;
		FPGA_CS_N		: in	std_logic;
		FPGA_IO			: inout	std_logic_vector(15 downto 0);
		FPGA_LED_RED_N 		: out	std_logic;
		FPGA_LED_GREEN_N 	: out	std_logic
	);
	end component;
--	component Localbus is
--	port (
--				CLK			: in		std_logic;
--				RESET_N		: in		std_logic;
--
--				MEM_ADDR		: in		std_logic_vector(25 downto 2);
--				MEM_DATA_I	: in		std_logic_vector(31 downto 0);
--				MEM_DATA_O	: out		std_logic_vector(31 downto 0);
--				MEM_DATA_T	: out		std_logic_vector(31 downto 0);
--				MEM_DQM		: in		std_logic_vector(3 downto 0);
--				MEM_CS_N		: in		std_logic;
--				MEM_WE_N		: in		std_logic;
--				MEM_OE_N		: in		std_logic;
--
--				LOC_ADDR		: out		std_logic_vector(25 downto 2);
--				LOC_DATA_I	: in		std_logic_vector(31 downto 0);
--				LOC_DATA_O	: out		std_logic_vector(31 downto 0);
--				LOC_DQM		: out		std_logic_vector(3 downto 0);
--				LOC_WR_N		: out		std_logic;
--				LOC_VALID_N	: out		std_logic
--			);
--	end component;



	SIGNAL sdclk0 			:  std_logic;
	SIGNAL sdclk1 			:  std_logic;
	SIGNAL reset_n 		:  std_logic;

	SIGNAL mem_addr 		:  std_logic_vector(25 downto 2);
	SIGNAL mem_data 		:  std_logic_vector(31 downto 0);
	SIGNAL mem_dqm 		:  std_logic_vector(3 downto 0);
	SIGNAL mem_cs_n		:  std_logic;
	SIGNAL mem_we_n 		:  std_logic;
	SIGNAL mem_oe_n 		:  std_logic;
	SIGNAL mem_rdwr_n		:  std_logic;

	SIGNAL fpga_io : std_logic_vector(15 downto 0);
	SIGNAL led_green, led_red 	:  std_logic;

BEGIN

	-- Instantiate the Unit Under Test (UUT)
	fpga_1 : FPGA_TOP
	port map (
		SDCLK0 => sdclk0,
		FPGA_RESET_N => reset_n,
		DATA => mem_data,
		ADDR => mem_addr,
		DQM => mem_dqm,
		OE_N => mem_oe_n,
		PWE_N	=> mem_we_n,
--		RDWR_N => mem_rdwr_n,
		FPGA_CS_N => mem_cs_n,
		FPGA_IO => fpga_io,
		FPGA_LED_RED_N => led_red,
		FPGA_LED_GREEN_N => led_green
	);

--	localb : Localbus
--	port map (
--		CLK => sdclk0,
--		RESET_N => reset_n,
--
--		MEM_ADDR => mem_addr,
--		MEM_DATA_I	=> mem_data_i,
--		MEM_DATA_O	=> mem_data_o,
--		MEM_DATA_T	=> mem_data_t,
--		MEM_DQM => mem_dqm,
--		MEM_CS_N => mem_cs_n,
--		MEM_WE_N => mem_we_n,
--		MEM_OE_N => mem_oe_n,
--
--		LOC_ADDR => loc_addr,
--		LOC_DATA_I => loc_data_i,
--		LOC_DATA_O => loc_data_o,
--		LOC_dqm => loc_dqm,
--		LOC_WR_N => loc_wr_n,
--		LOC_VALID_N	=> loc_valid_n
--	);

	takt : PROCESS
	BEGIN
		sdclk1 <= '1';		-- 200 MHz
		sdclk0 <= '1';		-- 50 MHz
		wait for 2.5 ns;
		sdclk1 <= '0';
		wait for 2.5 ns;
		sdclk1 <= '1';
		wait for 2.5 ns;
		sdclk1 <= '0';
		wait for 2.5 ns;
		sdclk1 <= '1';
		sdclk0 <= '0';
		wait for 2.5 ns;
		sdclk1 <= '0';
		wait for 2.5 ns;
		sdclk1 <= '1';
		wait for 2.5 ns;
		sdclk1 <= '0';
		wait for 2.5 ns;
	END PROCESS;

	tb_lbus : PROCESS
		-- RDF 7, RDN 2, RRR 1
		procedure burst_read(ADDR: in std_logic_vector(27 downto 0); COUNT: integer) is
		variable i,j: integer;
		-- this procedure simulates a single read into FPGA
		begin
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_rdwr_n <= '1';
			mem_addr <= ADDR(25 downto 2);
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_dqm <= (others => '0');
			mem_cs_n <= '0';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_oe_n <= '0';
			for j in 1 to 8 loop
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			end loop;
			for i in 1 to COUNT-1 loop
				mem_oe_n <= '1';
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				mem_addr <= ADDR(25 downto 2) + i;
				mem_dqm <= (others => '0');
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				mem_oe_n <= '0';
				for j in 1 to 8 loop
					wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				end loop;
			end loop;
			mem_oe_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_cs_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_dqm <= (others => 'Z');
			mem_addr <= (others => 'Z');
			mem_rdwr_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
		end procedure burst_read;

		procedure burst_write(ADDR: in std_logic_vector(27 downto 0); DATA: in std_logic_vector(31 downto 0); COUNT: integer) is
		-- this procedure simulates a burst write into FPGA
		variable i,j: integer;
		begin
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_addr <= ADDR(25 downto 2);
			mem_rdwr_n <= '0';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_cs_n <= '0';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_data <= DATA;
			mem_dqm <= (others => '0');
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_we_n <= '0';
			for j in 1 to 8 loop
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			end loop;
			for i in 1 to COUNT-1 loop
				mem_we_n <= '1';
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				mem_data <= DATA - i;
				mem_addr <= ADDR(25 downto 2) + i;
				mem_dqm <= (others => '0');
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				mem_we_n <= '0';
				for j in 1 to 8 loop
					wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
				end loop;
			end loop;
			mem_we_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_cs_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			mem_data <= (others => 'Z');
			mem_dqm <= (others => 'Z');
			mem_addr <= (others => 'Z');
			mem_rdwr_n <= '1';
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
		end procedure burst_write;

	variable j: integer;
	BEGIN
		mem_data <= (others => 'Z');
		mem_addr <= (others => 'Z');
		mem_dqm <= (others => 'Z');
		mem_oe_n <= '1';
		mem_we_n <= '1';
		mem_rdwr_n <= '1';
		mem_cs_n <= '1';
		reset_n <= '0';
		-- Wait 200 ns for global reset to finish
		wait for 200 ns;
		  reset_n <= '1';
		wait for 200 ns;
		-- Place stimulus here

		for j in 0 to 1 loop
			wait until (sdclk1'event and sdclk1 = '1'); -- wait until next clock event
		end loop;

		-- MEM
		burst_write(X"0000010", X"12345678", 1);
		burst_read(X"0000010", 1);
		-- IO
		burst_read(X"0000000", 1);
		burst_write(X"0000004", X"0000AAAA", 1);
		burst_write(X"0000008", X"0000FFFF", 1);
		burst_read(X"0000000", 1);

		wait; -- will wait forever
	END PROCESS;

END;
