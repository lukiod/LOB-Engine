#include <pybind11/pybind11.h>
#include "LOB/OrderBook.h"

namespace py = pybind11;

PYBIND11_MODULE(lob_core, m) {
    m.doc() = "High-Performance LOBSTER Limit Order Book Engine";

    py::enum_<LOB::Side>(m, "Side")
        .value("Buy", LOB::Side::Buy)
        .value("Sell", LOB::Side::Sell)
        .export_values();

    py::class_<LOB::OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def("add_order", &LOB::OrderBook::addOrder, "Add a new order")
        .def("cancel_order", &LOB::OrderBook::cancelOrder, "Cancel an order by ID")
        .def("delete_order", &LOB::OrderBook::deleteOrder, "Delete an order by ID (with fallback)")
        .def("execute_order", &LOB::OrderBook::executeOrder, "Execute an order by ID")
        .def("get_best_bid", &LOB::OrderBook::getBestBid, "Get Best Bid Price")
        .def("get_best_ask", &LOB::OrderBook::getBestAsk, "Get Best Ask Price")
        .def("get_obi", &LOB::OrderBook::getOBI, "Calculate Order Book Imbalance")
        .def("get_microprice", &LOB::OrderBook::getMicroprice, "Calculate Microprice");
}
