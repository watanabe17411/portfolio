package controller;

import dao.IotLogDao;
import model.IotLog;
import jakarta.servlet.ServletException;
import jakarta.servlet.annotation.WebServlet;
import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;

import java.io.IOException;
import java.util.List;

@WebServlet("/logs")
public class IotLogServlet extends HttpServlet {

	@Override
	protected void doGet(HttpServletRequest request, HttpServletResponse response)
			throws ServletException, IOException {

		String dbUrl = "jdbc:postgresql://127.0.0.1:5432/portfolio_db";
		String dbUser = "postgres";
		String dbPass = "postgres";

		IotLogDao dao = new IotLogDao(dbUrl, dbUser, dbPass);

		String viewType = request.getParameter("view");
		List<IotLog> logList;

		// viewパラメータが "all" の場合は全件、それ以外（recent等）は直近10件
		if ("all".equals(viewType)) {
			logList = dao.getAllLogs();
		} else {
			logList = dao.getRecent10Logs();
		}

		request.setAttribute("currentView", viewType);
		request.setAttribute("logs", logList);

		request.getRequestDispatcher("/WEB-INF/logs.jsp").forward(request, response);
	}
}
